#include "PluginProcessor.h"

#include "Constants.h"
#include "DrumMap.h"
#include "PluginEditor.h"
#include "Timing.h"

#include <algorithm>
#include <cmath>

// ── Parameter layout helpers ──
namespace {
void addChoice (std::vector<std::unique_ptr<juce::RangedAudioParameter>> &p, const char *id, const char *name,
                juce::StringArray choices, int def) {
  p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID{id, 1}, name, choices, def));
}
void addFloat (std::vector<std::unique_ptr<juce::RangedAudioParameter>> &p, const char *id, const char *name,
               juce::NormalisableRange<float> range, float def) {
  p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID{id, 1}, name, range, def));
}
void addInt (std::vector<std::unique_ptr<juce::RangedAudioParameter>> &p, const char *id, const char *name, int lo,
             int hi, int def) {
  p.push_back (std::make_unique<juce::AudioParameterInt> (juce::ParameterID{id, 1}, name, lo, hi, def));
}
void addBool (std::vector<std::unique_ptr<juce::RangedAudioParameter>> &p, const char *id, const char *name, bool def) {
  p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID{id, 1}, name, def));
}
} // namespace

MidiGridAnalyzerAudioProcessor::MidiGridAnalyzerAudioProcessor ()
    : AudioProcessor (BusesProperties ().withOutput ("Output", juce::AudioChannelSet::stereo (), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout ()) {
#if JUCE_STANDALONE_APPLICATION
  isStandaloneMode = true;
#else
  isStandaloneMode = juce::JUCEApplicationBase::isStandaloneApp ();
#endif

  auto commandLineArgs = juce::JUCEApplicationBase::getCommandLineParameterArray ();
  for (const auto &arg : commandLineArgs) {
    if (arg.containsIgnoreCase ("test") || arg.containsIgnoreCase ("demo")) {
      if (auto *param = apvts.getParameter ("test_mode")) {
        param->setValueNotifyingHost (1.0f);
      }
      break;
    }
  }

  if (isStandaloneMode) {
    remoteServer = std::make_unique<RemoteControlServer> (apvts);
    remoteServer->setCalibrationCallbacks (
        [this] { startCalibration (); }, [this] (bool add) { applyCalibrationResult (add); },
        [this] { cancelCalibration (); }, [this] { return getCalibrationStateJson (); });
    remoteServer->onDrillCommand = [this] (const juce::var &message) { return drillCommand (message); };
    remoteServer->drillIsActive = [this] { return isDrillActive (); };
    remoteServer->onClickSubdivision = [this] (int index) { setClickSubdivisionAndGrid (index); };
    remoteServer->getDrillJson = [this] { return getDrillStateJson (); };
    remoteServer->onDrillDisconnect = [this] {
      juce::DynamicObject::Ptr message = new juce::DynamicObject ();
      message->setProperty ("action", "disconnect");
      drillCommand (juce::var (message.get ()));
    };
    remoteServer->start ();
  }
}

MidiGridAnalyzerAudioProcessor::~MidiGridAnalyzerAudioProcessor () {
  if (remoteServer) {
    remoteServer->stop ();
  }
}

juce::AudioProcessorValueTreeState::ParameterLayout MidiGridAnalyzerAudioProcessor::createParameterLayout () {
  std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

  addChoice (params, "bars_window", "History Length", {"1 Bar", "2 Bars", "4 Bars", "8 Bars"}, 2);
  addChoice (params, "subdivision", "Grid Subdivision", {"1/8", "1/8T", "1/16", "1/16T", "1/32"}, 2);
  addFloat (params, "tolerance_ms", "Timing Tolerance",
            {constants::params::toleranceMin, constants::params::toleranceMax, constants::params::toleranceStep},
            constants::params::toleranceDefault);
  addFloat (params, "latency_offset_ms", "System Latency Offset",
            {constants::params::latencyMin, constants::params::latencyMax, constants::params::latencyStep},
            constants::params::latencyDefault);
  addInt (params, "min_velocity", "Velocity Noise Floor", constants::params::velMin, constants::params::velMax,
          constants::params::velDefault);
  addFloat (params, "internal_bpm", "Internal BPM",
            {constants::params::bpmMin, constants::params::bpmMax, constants::params::bpmStep},
            constants::params::bpmDefault);
  addInt (params, "time_sig_num", "Time Sig Numerator", constants::params::timeSigMin, constants::params::timeSigMax,
          constants::params::timeSigDefault);
  addChoice (params, "click_subdivision", "Click Subdivision",
             {"Off", "1/4 Notes", "1/8 Notes", "1/16 Notes", "Triplets"}, 1);
  addChoice (params, "click_sample_preset", "Click Sound Preset", {"Wood Clave", "Drum Stick Click", "Digital Beep"},
             0);
  addFloat (params, "click_volume", "Click Volume",
            {constants::params::clickVolMin, constants::params::clickVolMax, constants::params::clickVolStep},
            constants::params::clickVolDefault);
  addFloat (params, "click_pan", "Click Panning",
            {constants::params::panMin, constants::params::panMax, constants::params::panStep},
            constants::params::panDefault);
  addBool (params, "click_enabled", "Metronome On/Off", true);
  addBool (params, "is_paused", "Pause/Freeze Grid", false);
  addBool (params, "show_ms_labels", "Display MS Offsets", true);
  addBool (params, "show_velocity_labels", "Display Velocity", false);
  addBool (params, "show_note_numbers", "Display Note Numbers", false);
  addBool (params, "test_mode", "Rock Beat Demo Mode", false);
  addChoice (params, "note_filter", "Display Mode", {"All Notes", "Roland/GM Drum Map", "Custom"}, 1);

  return {params.begin (), params.end ()};
}

double MidiGridAnalyzerAudioProcessor::getSubdivisionPpq (int index) noexcept {
  using namespace constants::musical;
  switch (index) {
  case 0:
    return ppq_1_8;
  case 1:
    return ppq_1_8T;
  case 2:
    return ppq_1_16;
  case 3:
    return ppq_1_16T;
  case 4:
    return ppq_1_32;
  default:
    return ppq_default;
  }
}

double MidiGridAnalyzerAudioProcessor::getClickSubdivisionPpq (int index) noexcept {
  using namespace constants::musical;
  switch (index) {
  case 1:
    return ppq_click_1_4;
  case 2:
    return ppq_click_1_8;
  case 3:
    return ppq_click_1_16;
  case 4:
    return ppq_click_triplet;
  default:
    return 0.0;
  }
}

double MidiGridAnalyzerAudioProcessor::getEffectiveGridInterval (const ParamSnapshot &p) noexcept {
  // Drummer hears click subdivisions — that's the grid that matters for timing.
  // If click is enabled and has a subdivision, use that; otherwise fall back to display grid.
  if (p.clickEnabled) {
    const double clickPpq = getClickSubdivisionPpq (p.clickSubChoice);
    if (clickPpq > 0.0) {
      return clickPpq;
    }
  }
  return getSubdivisionPpq (p.subChoice);
}

MidiGridAnalyzerAudioProcessor::ParamSnapshot
MidiGridAnalyzerAudioProcessor::readSnapshot (const juce::AudioProcessorValueTreeState &state) noexcept {
  ParamSnapshot s;
  s.subChoice = static_cast<int> (state.getRawParameterValue ("subdivision")->load ());
  s.toleranceMs = state.getRawParameterValue ("tolerance_ms")->load ();
  s.userLatencyMs = state.getRawParameterValue ("latency_offset_ms")->load ();
  s.minVelocity = static_cast<int> (state.getRawParameterValue ("min_velocity")->load ());
  s.internalBpm = state.getRawParameterValue ("internal_bpm")->load ();
  s.timeSigNum = static_cast<int> (state.getRawParameterValue ("time_sig_num")->load ());
  s.clickSubChoice = static_cast<int> (state.getRawParameterValue ("click_subdivision")->load ());
  s.clickPreset = static_cast<int> (state.getRawParameterValue ("click_sample_preset")->load ());
  s.clickVolume = state.getRawParameterValue ("click_volume")->load ();
  s.clickPan = state.getRawParameterValue ("click_pan")->load ();
  s.clickEnabled = state.getRawParameterValue ("click_enabled")->load () > 0.5f;
  s.isPaused = state.getRawParameterValue ("is_paused")->load () > 0.5f;
  s.testMode = state.getRawParameterValue ("test_mode")->load () > 0.5f;
  return s;
}

void MidiGridAnalyzerAudioProcessor::setDeviceLatencySamples (int outputLatencySamples,
                                                              int inputLatencySamples) noexcept {
  deviceOutputLatencySamples.store (outputLatencySamples);
  deviceInputLatencySamples.store (inputLatencySamples);
}

double MidiGridAnalyzerAudioProcessor::getDeviceLatencyMs (double sampleRate) const noexcept {
  if (sampleRate <= 0.0) {
    return 0.0;
  }
  // For e-kit USB MIDI, input latency is negligible; output latency (click) dominates.
  // Expose output latency for compensation; input kept for future audio-input path.
  return (static_cast<double> (deviceOutputLatencySamples.load ()) / sampleRate) * 1000.0;
}

void MidiGridAnalyzerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock) {
  if (isStandaloneMode && samplesPerBlock > 0) {
    // Heuristic fallback so audio thread has correct total latency even without editor open.
    // Real device latency is refined by PluginEditor::updateDeviceLatency() polling.
    deviceOutputLatencySamples.store (samplesPerBlock);
    deviceInputLatencySamples.store (0);
  }
  internalPpqPosition = 0.0;
  if (drill.active ()) {
    drill.command (DrillEngine::Action::Pause, drill.view.config, 0);
    publishDrillSnapshot ();
  }
  lastTestBeatTick = -1.0;
  ringBuffer.reset ();
  clickGenerator.prepareToPlay (sampleRate);
}

void MidiGridAnalyzerAudioProcessor::releaseResources () {}

bool MidiGridAnalyzerAudioProcessor::isBusesLayoutSupported (const BusesLayout &layouts) const {
  if (layouts.getMainOutputChannelSet () != juce::AudioChannelSet::mono () &&
      layouts.getMainOutputChannelSet () != juce::AudioChannelSet::stereo ())
    return false;
  return true;
}

void MidiGridAnalyzerAudioProcessor::processAudioChunk (juce::AudioBuffer<float> &buffer,
                                                        juce::MidiBuffer &midiMessages) {
  buffer.clear ();

  const double sampleRate = getSampleRate ();
  const int numSamples = buffer.getNumSamples ();
  ParamSnapshot p = readSnapshot (apvts);
  if (drill.active ()) {
    p.internalBpm = static_cast<float> (drill.view.bpm);
    p.timeSigNum = drill.view.config.beats;
    p.isPaused = !drill.running ();
    p.clickEnabled = true;
    p.clickSubChoice = drill.view.config.clickSubdivisionIndex ();
    p.testMode = false;
    p.minVelocity = drill.view.config.minVelocity;
    p.toleranceMs = static_cast<float> (drill.view.toleranceMs);
  }
  const double gridInterval = drill.active () ? drill.view.config.interval : getSubdivisionPpq (p.subChoice);

  updateHostSyncAndPlayhead (p.internalBpm, p.timeSigNum, p.isPaused);

  const double srToUse = (sampleRate > 0.0) ? sampleRate : constants::params::sampleRateFallback;
  const double blockStartPpq = currentPpqPosition;
  const double blockEndPpq = blockStartPpq + numSamples * (currentBpm / 60.0) / srToUse;

  // Calibration state transitions (count-in -> recording -> done)
  updateCalibrationState (blockStartPpq, blockEndPpq);

  if (isStandaloneMode && !p.isPaused) {
    clickGenerator.renderBlock (buffer, numSamples, srToUse, blockStartPpq, currentBpm, currentTimeSigNum,
                                p.clickSubChoice, p.clickPreset, p.clickVolume, p.clickPan, p.clickEnabled);
  }

  const double autoLatencyMs = (static_cast<double> (getLatencySamples ()) / srToUse) * 1000.0;
  const double deviceLatencyMs = getDeviceLatencyMs (srToUse);
  const double userLatencyMs = static_cast<double> (p.userLatencyMs);
  const double totalLatencyMs =
      drill.active () ? drill.view.config.latencyMs : autoLatencyMs + deviceLatencyMs + userLatencyMs;
  const double totalLatencyPpq = (totalLatencyMs / 1000.0) * (currentBpm / 60.0);
  // For calibration: measure absolute offset EXCLUDING current user latency
  // (so result can replace, not add). Use snapshot calibBpm if calibrating.
  const double calibLatencyMs = autoLatencyMs + deviceLatencyMs;
  const double calibBpmForPpq = (calibState.load () != static_cast<int> (CalibState::Idle)) ? calibBpm : currentBpm;
  const double calibLatencyPpq = (calibLatencyMs / 1000.0) * (calibBpmForPpq / 60.0);

  if (!p.isPaused) {
    processIncomingMidi (midiMessages, srToUse, gridInterval, p.toleranceMs, p.minVelocity, totalLatencyPpq,
                         calibLatencyPpq);
  }

  if (p.testMode && !p.isPaused && currentTimeSigNum > 0) {
    generateTestModeBeat (blockStartPpq, blockEndPpq, totalLatencyPpq, gridInterval, p.toleranceMs);
  }

  if (!p.isPaused) {
    internalPpqPosition += numSamples * (currentBpm / 60.0) / srToUse;
  }
}

void MidiGridAnalyzerAudioProcessor::updateHostSyncAndPlayhead (float internalBpmVal, int timeSigNumVal,
                                                                bool isPausedVal) {
  bool hostPpqValid = false;
  double hostBpm = internalBpmVal;
  double hostPpq = internalPpqPosition;
  int hostTimeSigNum = timeSigNumVal;
  bool hostPlaying = false;

  if (auto playHead = isStandaloneMode ? nullptr : getPlayHead ()) {
    if (auto pos = playHead->getPosition ()) {
      if (auto bpmOpt = pos->getBpm ()) {
        if (*bpmOpt > 0.0) {
          hostBpm = *bpmOpt;
        }
      }
      if (auto ppqOpt = pos->getPpqPosition ()) {
        hostPpq = *ppqOpt;
        hostPpqValid = true;
      }
      if (auto tsOpt = pos->getTimeSignature ()) {
        if (tsOpt->numerator > 0) {
          hostTimeSigNum = tsOpt->numerator;
        }
      }
      hostPlaying = pos->getIsPlaying ();
    }
  }

  currentBpm = (hostBpm > 0.0) ? hostBpm : internalBpmVal;
  currentPpqPosition = hostPpqValid ? hostPpq : internalPpqPosition;
  currentTimeSigNum = (hostTimeSigNum > 0) ? hostTimeSigNum : timeSigNumVal;
  hostIsPlaying = (hostPlaying || !hostPpqValid) && !isPausedVal;
}

bool MidiGridAnalyzerAudioProcessor::updateHiHatHistoryAndShouldFilter (uint8_t noteNum, uint8_t velocity,
                                                                        double nowMs) {
  const bool isOtherHiHat = (noteNum == DrumMap::ClosedHiHatEdge || noteNum == DrumMap::ClosedHiHat ||
                             noteNum == DrumMap::PedalHiHat || noteNum == DrumMap::OpenHiHatEdge);

  if (isOtherHiHat) {
    lastOtherHiHatTimeMs = nowMs;
    return false;
  }

  if (noteNum == DrumMap::OpenHiHat) {
    const double windowMs = DrumMap::hiHatDebounceWindowMs (velocity);
    if ((nowMs - lastOtherHiHatTimeMs) < windowMs) {
      return true;
    }
  }

  return false;
}

void MidiGridAnalyzerAudioProcessor::processIncomingMidi (const juce::MidiBuffer &midiMessages, double srToUse,
                                                          double gridInterval, float toleranceMs, int minVelocity,
                                                          double totalLatencyPpq, double calibLatencyPpq) {
  const bool isCalibRec = (calibState.load () == static_cast<int> (CalibState::Recording));
  for (const auto metadata : midiMessages) {
    if (metadata.samplePosition < drillMidiOffset || metadata.samplePosition >= drillMidiOffset + drillMidiCount) {
      continue;
    }
    const auto msg = metadata.getMessage ();
    if (!(msg.isNoteOn () && msg.getVelocity () >= minVelocity &&
          !DrumMap::isExcluded (static_cast<uint8_t> (msg.getNoteNumber ()))))
      continue;

    const uint8_t noteNum = static_cast<uint8_t> (msg.getNoteNumber ());
    const uint8_t velocity = static_cast<uint8_t> (msg.getVelocity ());
    const double nowMs = juce::Time::getMillisecondCounterHiRes ();

    if (updateHiHatHistoryAndShouldFilter (noteNum, velocity, nowMs)) {
      continue;
    }

    const int sampleOffset = metadata.samplePosition - drillMidiOffset;
    const double rawHitPpq = currentPpqPosition + (sampleOffset * (currentBpm / 60.0) / srToUse);
    const double compensatedHitPpq = rawHitPpq - totalLatencyPpq;
    if (drill.active ()) {
      drill.hit (compensatedHitPpq, noteNum);
    }
    const auto timing = Timing::compute (compensatedHitPpq, gridInterval, currentBpm, toleranceMs);

    HitEvent event;
    event.noteNumber = static_cast<uint8_t> (msg.getNoteNumber ());
    event.velocity = static_cast<uint8_t> (msg.getVelocity ());
    event.rawHitPpqPosition = rawHitPpq;
    event.hitPpqPosition = compensatedHitPpq;
    event.deltaMs = timing.deltaMs;
    event.normalizedDeviation = timing.normalizedDeviation;
    event.state = timing.state;
    ringBuffer.push (event);

    // Calibration capture: measure absolute offset EXCLUDING current user latency
    // (so result can replace user latency, not add to it). Use raw - (auto+device) only.
    // Count hits whose nearest calibration grid lies within the recording window,
    // so early/late hits just outside window (rush/drag) are still counted.
    if (isCalibRec) {
      const double calibCompPpq = rawHitPpq - calibLatencyPpq;
      const double nearestGrid = std::round (calibCompPpq / calibGridInterval) * calibGridInterval;
      if (nearestGrid >= calibRecStartPpq - 1e-9 && nearestGrid < calibRecEndPpq - 1e-9) {
        const auto calibTiming = Timing::compute (calibCompPpq, calibGridInterval, calibBpm, toleranceMs);
        std::lock_guard<std::mutex> lock (calibMutex);
        calibDeltas.push_back (calibTiming.deltaMs);
      }
    }
  }
}

// ── Test-mode helpers ──
double MidiGridAnalyzerAudioProcessor::generateHumanizedDeviationMs (float toleranceMs) {
  const float roll = random.nextFloat ();
  if (roll < 0.80f) {
    const double factor = static_cast<double> (random.nextFloat () * 1.4f - 0.7f);
    return factor * static_cast<double> (toleranceMs);
  }
  if (roll < 0.90f) {
    const double factor = static_cast<double> (1.25f + random.nextFloat () * 0.95f);
    return -factor * static_cast<double> (toleranceMs);
  }
  const double factor = static_cast<double> (1.25f + random.nextFloat () * 0.95f);
  return factor * static_cast<double> (toleranceMs);
}

HitEvent MidiGridAnalyzerAudioProcessor::makeQuantizedHit (uint8_t note, uint8_t vel, double targetCompPpq,
                                                           double gridInterval, double bpm, float toleranceMs,
                                                           double totalLatencyPpq) const {
  const double rawPpq = targetCompPpq + totalLatencyPpq;
  const auto timing = Timing::compute (targetCompPpq, gridInterval, bpm, toleranceMs);
  HitEvent e;
  e.noteNumber = note;
  e.velocity = vel;
  e.rawHitPpqPosition = rawPpq;
  e.hitPpqPosition = targetCompPpq;
  e.deltaMs = timing.deltaMs;
  e.normalizedDeviation = timing.normalizedDeviation;
  e.state = timing.state;
  return e;
}

void MidiGridAnalyzerAudioProcessor::generateTestModeBeat (double blockStartPpq, double blockEndPpq,
                                                           double totalLatencyPpq, double gridInterval,
                                                           float toleranceMs) {
  constexpr double subInterval = 0.5; // 8th notes
  const double firstTick = std::floor (blockStartPpq / subInterval) * subInterval;

  for (double tick = firstTick; tick < blockEndPpq; tick += subInterval) {
    if (tick < blockStartPpq || tick <= lastTestBeatTick) {
      continue;
    }

    lastTestBeatTick = tick;

    const double barPpq = std::fmod (tick, static_cast<double> (currentTimeSigNum));
    const int beatInBar = static_cast<int> (std::floor (barPpq));
    const double subFraction = std::fmod (tick, 1.0);
    const bool is8thAnd = (std::abs (subFraction - 0.5) < 0.001);

    auto emit = [this, tick, totalLatencyPpq, gridInterval, toleranceMs] (uint8_t note, uint8_t vel) {
      const double devMs = generateHumanizedDeviationMs (toleranceMs);
      const double devPpq = (devMs / 1000.0) * (currentBpm / 60.0);
      const double targetCompPpq = tick + devPpq;
      ringBuffer.push (
          makeQuantizedHit (note, vel, targetCompPpq, gridInterval, currentBpm, toleranceMs, totalLatencyPpq));
      // Also feed calibration if in recording window (test_mode hit also counts)
      if (calibState.load () == static_cast<int> (CalibState::Recording) && note == DrumMap::ClosedHiHat) {
        const double nearest = std::round (targetCompPpq / calibGridInterval) * calibGridInterval;
        if (nearest >= calibRecStartPpq - 1e-9 && nearest < calibRecEndPpq - 1e-9) {
          const auto ct = Timing::compute (targetCompPpq, calibGridInterval, calibBpm, toleranceMs);
          std::lock_guard<std::mutex> lock (calibMutex);
          calibDeltas.push_back (ct.deltaMs);
        }
      }
    };

    emit (DrumMap::ClosedHiHat, static_cast<uint8_t> (DrumMap::TestModeVelocity::BaseHiHat +
                                                      random.nextInt (DrumMap::TestModeVelocity::HiHatRange)));

    if (!is8thAnd && (beatInBar == 0 || beatInBar == 2)) {
      emit (DrumMap::Kick, static_cast<uint8_t> (DrumMap::TestModeVelocity::BaseKick +
                                                 random.nextInt (DrumMap::TestModeVelocity::KickRange)));
    }

    if (!is8thAnd && (beatInBar == 1 || beatInBar == 3)) {
      emit (DrumMap::SnareHead, static_cast<uint8_t> (DrumMap::TestModeVelocity::BaseSnare +
                                                      random.nextInt (DrumMap::TestModeVelocity::SnareRange)));
    }

    if (!is8thAnd && beatInBar == 0 &&
        std::abs (std::fmod (tick, static_cast<double> (currentTimeSigNum) * 4.0)) < 0.001)
      emit (DrumMap::Crash1, DrumMap::TestModeVelocity::Crash);
  }
}

// ── Calibration wizard ──
void MidiGridAnalyzerAudioProcessor::startCalibration () {
  if (isDrillActive ()) {
    return;
  }
  const ParamSnapshot p = readSnapshot (apvts);
  const double effectiveIntervalForCalib = getEffectiveGridInterval (p);
  if (effectiveIntervalForCalib <= 0.0 || p.timeSigNum <= 0 || currentBpm <= 0.0) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock (calibMutex);
    calibDeltas.clear ();
    calibResult = {};
  }
  calibBpm = currentBpm;
  calibGridInterval = effectiveIntervalForCalib;
  calibTimeSigNum = p.timeSigNum;
  // Grid-synced: start at next bar line (beat 1), so count-in is exactly 1 bar
  // e.g., if now at 2.3 and 4/4, next bar is 4.0, count 4..1, then rec at 8.0
  const double nextBar =
      std::ceil (currentPpqPosition / static_cast<double> (calibTimeSigNum)) * static_cast<double> (calibTimeSigNum);
  // If already exactly on bar line, start now (no extra gap)
  calibStartPpq = (std::abs (currentPpqPosition - nextBar) < 1e-9) ? currentPpqPosition : nextBar;
  // Count-in: 1 bar of quarters (timeSigNum beats), Recording: 4 bars of subdivisions
  calibCountInEndPpq = calibStartPpq + static_cast<double> (calibTimeSigNum);
  calibRecStartPpq = calibCountInEndPpq;
  calibRecEndPpq = calibRecStartPpq + 4.0 * static_cast<double> (calibTimeSigNum);
  const double totalPpq = 4.0 * static_cast<double> (calibTimeSigNum);
  calibExpectedHits = static_cast<int> (std::round (totalPpq / calibGridInterval));
  calibState.store (static_cast<int> (CalibState::CountIn));
  calibNeedsBroadcast.store (true);
}

void MidiGridAnalyzerAudioProcessor::cancelCalibration () {
  calibState.store (static_cast<int> (CalibState::Idle));
  {
    std::lock_guard<std::mutex> lock (calibMutex);
    calibDeltas.clear ();
    calibResult = {};
  }
  calibNeedsBroadcast.store (true);
}

void MidiGridAnalyzerAudioProcessor::applyCalibrationResult (bool addToExisting) {
  CalibResult r;
  {
    std::lock_guard<std::mutex> lock (calibMutex);
    r = calibResult;
  }
  if (!r.hasResult || r.hitCount == 0) {
    return;
  }
  const float currentUser = apvts.getRawParameterValue ("latency_offset_ms")->load ();
  const float newVal = addToExisting ? currentUser + static_cast<float> (r.meanMs) : static_cast<float> (r.meanMs);
  const float clamped = std::clamp (newVal, constants::params::latencyMin, constants::params::latencyMax);
  if (auto *param = apvts.getParameter ("latency_offset_ms")) {
    param->setValueNotifyingHost (param->convertTo0to1 (clamped));
  }
  // Reset to idle after apply (keep result for display until next start)
  calibState.store (static_cast<int> (CalibState::Idle));
  calibNeedsBroadcast.store (true);
}

MidiGridAnalyzerAudioProcessor::CalibResult MidiGridAnalyzerAudioProcessor::getCalibrationResult () const {
  std::lock_guard<std::mutex> lock (const_cast<std::mutex &> (calibMutex));
  return calibResult;
}

int MidiGridAnalyzerAudioProcessor::getCalibrationBeatsRemaining () const noexcept {
  const auto st = static_cast<CalibState> (calibState.load ());
  if (st == CalibState::CountIn) {
    const double remaining = calibCountInEndPpq - currentPpqPosition;
    return std::max (0, static_cast<int> (std::ceil (remaining)));
  }
  if (st == CalibState::Recording) {
    const double remaining = calibRecEndPpq - currentPpqPosition;
    return std::max (0, static_cast<int> (std::ceil (remaining)));
  }
  return 0;
}

double MidiGridAnalyzerAudioProcessor::getCalibrationProgress () const noexcept {
  const auto st = static_cast<CalibState> (calibState.load ());
  if (st == CalibState::CountIn) {
    const double total = calibCountInEndPpq - calibStartPpq;
    const double done = currentPpqPosition - calibStartPpq;
    return total > 0.0 ? std::clamp (done / total, 0.0, 1.0) : 0.0;
  }
  if (st == CalibState::Recording) {
    const double total = calibRecEndPpq - calibRecStartPpq;
    const double done = currentPpqPosition - calibRecStartPpq;
    return total > 0.0 ? std::clamp (done / total, 0.0, 1.0) : 0.0;
  }
  if (st == CalibState::Done) {
    return 1.0;
  }
  return 0.0;
}

void MidiGridAnalyzerAudioProcessor::updateCalibrationState (double blockStartPpq, double blockEndPpq) {
  const auto st = static_cast<CalibState> (calibState.load ());
  if (st == CalibState::CountIn) {
    if (blockEndPpq >= calibCountInEndPpq) {
      calibState.store (static_cast<int> (CalibState::Recording));
      calibNeedsBroadcast.store (true);
    }
  } else if (st == CalibState::Recording) {
    if (blockEndPpq >= calibRecEndPpq) {
      finalizeCalibration ();
    }
  }
  juce::ignoreUnused (blockStartPpq);
}

void MidiGridAnalyzerAudioProcessor::finalizeCalibration () {
  std::vector<double> deltas;
  {
    std::lock_guard<std::mutex> lock (calibMutex);
    deltas = calibDeltas;
  }
  CalibResult r;
  r.expectedHits = calibExpectedHits;
  r.hitCount = static_cast<int> (deltas.size ());
  r.hasResult = (r.hitCount > 0);
  if (r.hitCount > 0) {
    double sum = 0.0;
    for (double d : deltas) {
      sum += d;
    }
    r.meanMs = sum / deltas.size ();
    // Median
    std::vector<double> sorted = deltas;
    std::sort (sorted.begin (), sorted.end ());
    if (sorted.size () % 2 == 1) {
      r.medianMs = sorted[sorted.size () / 2];
    } else {
      r.medianMs = (sorted[sorted.size () / 2 - 1] + sorted[sorted.size () / 2]) * 0.5;
    }
    // SD (outlier trim: ignore >2*SD in mean? keep simple SD)
    double var = 0.0;
    for (double d : deltas) {
      var += (d - r.meanMs) * (d - r.meanMs);
    }
    r.sdMs = std::sqrt (var / deltas.size ());
    // Trim outliers >2*SD for final mean (robust)
    if (r.sdMs > 0.0 && deltas.size () >= 4) {
      double trimmedSum = 0.0;
      int trimmedCount = 0;
      for (double d : deltas) {
        if (std::abs (d - r.meanMs) <= 2.0 * r.sdMs) {
          trimmedSum += d;
          ++trimmedCount;
        }
      }
      if (trimmedCount >= 2) {
        r.meanMs = trimmedSum / trimmedCount;
      }
    }
    // Corner cases: keep old latency — record reason for UI
    if (r.hitCount == 0) {
      r.failReason = "noHits";
    } else if (r.hasResult) {
      const bool tooFew = r.hitCount < (r.expectedHits * 0.5);
      const bool jitterHigh = r.sdMs > 20.0;
      if (tooFew) {
        r.hasResult = false;
        r.failReason = "tooFew";
      } else if (jitterHigh) {
        r.hasResult = false;
        r.failReason = "jitter";
      } else {
        r.failReason = "";
      }
    }
    // Clamp negative (rush) to 0 — latency cannot be negative
    if (r.hasResult && r.meanMs < 0.0) {
      r.meanMs = 0.0;
      r.medianMs = std::max (0.0, r.medianMs);
    }
  }
  {
    std::lock_guard<std::mutex> lock (calibMutex);
    calibResult = r;
  }
  calibState.store (static_cast<int> (CalibState::Done));
  calibNeedsBroadcast.store (true);
}

juce::String MidiGridAnalyzerAudioProcessor::getCalibrationStateJson () const {
  const auto st = static_cast<CalibState> (calibState.load ());
  const bool needs = calibNeedsBroadcast.load ();
  // Idle: only broadcast when state just changed to idle (needs flag)
  if (st == CalibState::Idle) {
    if (!needs) {
      return {};
    }
    const_cast<std::atomic<bool> &> (calibNeedsBroadcast).store (false);
  } else if (st == CalibState::CountIn || st == CalibState::Recording) {
    // Live progress during count-in/recording — broadcast every poll (10Hz)
    // so companion shows hit count and progress continuously.
    // Don't clear needs flag here; keep it for Done transition.
  } else if (st == CalibState::Done) {
    if (!needs) {
      return {};
    }
    const_cast<std::atomic<bool> &> (calibNeedsBroadcast).store (false);
  }
  const char *stateStr = "idle";
  if (st == CalibState::CountIn) {
    stateStr = "countin";
  } else if (st == CalibState::Recording) {
    stateStr = "recording";
  } else if (st == CalibState::Done) {
    stateStr = "done";
  }
  auto obj = juce::DynamicObject::Ptr (new juce::DynamicObject ());
  obj->setProperty ("type", "calibration");
  obj->setProperty ("state", stateStr);
  obj->setProperty ("progress", getCalibrationProgress ());
  obj->setProperty ("beatsRemaining", getCalibrationBeatsRemaining ());
  // Live hit count during recording vs final result when done
  int liveHits = 0;
  {
    std::lock_guard<std::mutex> lock (const_cast<std::mutex &> (calibMutex));
    liveHits = static_cast<int> (calibDeltas.size ());
  }
  CalibResult r = getCalibrationResult ();
  // During recording, report live hits; when done, report final result
  const int hitCountToReport = (st == CalibState::Done) ? r.hitCount : liveHits;
  const bool hasResToReport = (st == CalibState::Done) ? r.hasResult : false;
  obj->setProperty ("meanMs", r.meanMs);
  obj->setProperty ("medianMs", r.medianMs);
  obj->setProperty ("sdMs", r.sdMs);
  obj->setProperty ("hitCount", hitCountToReport);
  obj->setProperty ("expectedHits", r.expectedHits > 0 ? r.expectedHits : calibExpectedHits);
  obj->setProperty ("hasResult", hasResToReport);
  obj->setProperty ("reason", r.failReason);
  // Include grid context for UI
  obj->setProperty ("bpm", calibBpm);
  obj->setProperty ("gridInterval", calibGridInterval);
  obj->setProperty ("timeSigNum", calibTimeSigNum);
  return juce::JSON::toString (obj.get ());
}

juce::AudioProcessorEditor *MidiGridAnalyzerAudioProcessor::createEditor () {
  return new MidiGridAnalyzerAudioProcessorEditor (*this);
}

void MidiGridAnalyzerAudioProcessor::getStateInformation (juce::MemoryBlock &destData) {
  auto state = apvts.copyState ();
  std::unique_ptr<juce::XmlElement> xml (state.createXml ());
  copyXmlToBinary (*xml, destData);
}

void MidiGridAnalyzerAudioProcessor::setStateInformation (const void *data, int sizeInBytes) {
  std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
  if (xmlState.get () != nullptr) {
    if (xmlState->hasTagName (apvts.state.getType ())) {
      apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
    }
  }
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter () {
  return new MidiGridAnalyzerAudioProcessor ();
}

void MidiGridAnalyzerAudioProcessor::consumeDrillCommand () {
  std::unique_lock<std::mutex> lock (drillMutex, std::try_to_lock);
  if (!lock.owns_lock ()) {
    return;
  }
  while (drillCommandRead != drillCommandWrite) {
    const auto &command = drillCommands[static_cast<size_t> (drillCommandRead)];
    drill.command (command.action, command.config, internalPpqPosition);
    drillCommandRead = (drillCommandRead + 1) % static_cast<int> (drillCommands.size ());
  }
}

void MidiGridAnalyzerAudioProcessor::publishDrillSnapshot () {
  std::unique_lock<std::mutex> lock (drillMutex, std::try_to_lock);
  if (lock.owns_lock ()) {
    drillSnapshot = drill.view;
  }
}

void MidiGridAnalyzerAudioProcessor::processBlock (juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) {
  consumeDrillCommand ();
  drillMidiOffset = 0;
  drillMidiCount = buffer.getNumSamples ();
  if (!drill.active ()) {
    processAudioChunk (buffer, midiMessages);
    publishDrillSnapshot ();
    return;
  }
  const double sr = getSampleRate () > 0 ? getSampleRate () : constants::params::sampleRateFallback;
  int offset = 0;
  while (offset < buffer.getNumSamples ()) {
    const double latencyPpq = drill.view.config.latencyMs * drill.view.bpm / 60000.0;
    drill.advance (internalPpqPosition, internalPpqPosition - latencyPpq);
    int count = buffer.getNumSamples () - offset;
    if (drill.running () && std::isfinite (drill.nextBoundary ())) {
      const double distance = (drill.nextBoundary () - internalPpqPosition) * sr * 60.0 / drill.view.bpm;
      count = std::min (count, std::max (1, static_cast<int> (std::ceil (distance - 1e-9))));
    }
    juce::AudioBuffer<float> chunk (buffer.getArrayOfWritePointers (), buffer.getNumChannels (), offset, count);
    drillMidiOffset = offset;
    drillMidiCount = count;
    processAudioChunk (chunk, midiMessages);
    drill.advance (internalPpqPosition, internalPpqPosition - latencyPpq);
    offset += count;
  }
  publishDrillSnapshot ();
}

DrillEngine::Snapshot MidiGridAnalyzerAudioProcessor::getDrillSnapshot () const {
  std::lock_guard<std::mutex> lock (drillMutex);
  return drillSnapshot;
}

bool MidiGridAnalyzerAudioProcessor::isDrillActive () const {
  std::lock_guard<std::mutex> lock (drillMutex);
  return drillSessionRequested;
}

bool MidiGridAnalyzerAudioProcessor::parseDrillConfig (const juce::var &message, DrillEngine::Config &config) const {
  const auto pattern = message["pattern"].toString ().removeCharacters (" \t\r\n").toUpperCase ();
  if (pattern.isEmpty () || pattern.length () > constants::drill::maxPattern || !pattern.containsOnly ("RLK")) {
    return false;
  }
  if (!message.hasProperty ("spacing") || !message.hasProperty ("bpm")) {
    return false;
  }
  const int spacing = static_cast<int> (message["spacing"]);
  const double bpm = static_cast<double> (message["bpm"]);
  const int kick = message.hasProperty ("kick") ? static_cast<int> (message["kick"]) : DrumMap::Kick;
  if (spacing < 0 || spacing > 2 || static_cast<double> (message["spacing"]) != spacing || !std::isfinite (bpm) ||
      bpm < constants::params::bpmMin || bpm > constants::params::bpmMax || kick < 0 || kick > 127 ||
      kick == DrumMap::PedalHiHat) {
    return false;
  }
  if (message.hasProperty ("kick") && static_cast<double> (message["kick"]) != kick) {
    return false;
  }
  config.length = pattern.length ();
  pattern.copyToUTF8 (config.pattern.data (), config.pattern.size ());
  config.bpm = bpm;
  config.kick = kick;
  config.interval = spacing == 0   ? constants::musical::ppq_1_8
                    : spacing == 1 ? constants::musical::ppq_1_8T
                                   : constants::musical::ppq_1_16;
  const auto params = readSnapshot (apvts);
  config.beats = params.timeSigNum;
  config.minVelocity = params.minVelocity;
  config.tolerance = message.hasProperty ("tolerance") ? static_cast<float> (message["tolerance"]) : params.toleranceMs;
  if (!std::isfinite (config.tolerance) || config.tolerance < constants::params::toleranceMin ||
      config.tolerance > constants::params::toleranceMax) {
    return false;
  }
  config.passThreshold = message.hasProperty ("passThreshold") ? static_cast<double> (message["passThreshold"])
                                                               : constants::drill::passThresholdDefault;
  if (!std::isfinite (config.passThreshold) || config.passThreshold < constants::drill::passThresholdMin ||
      config.passThreshold > constants::drill::passThresholdMax) {
    return false;
  }
  config.stepBpm = message.hasProperty ("step") ? static_cast<double> (message["step"]) : constants::drill::bpmStep;
  if (!std::isfinite (config.stepBpm) || config.stepBpm < constants::drill::bpmStepMin ||
      config.stepBpm > constants::drill::bpmStepMax) {
    return false;
  }
  const double sr = getSampleRate () > 0 ? getSampleRate () : constants::params::sampleRateFallback;
  config.latencyMs = params.userLatencyMs + getDeviceLatencyMs (sr) + getLatencySamples () * 1000.0 / sr;
  return true;
}

bool MidiGridAnalyzerAudioProcessor::drillCommand (const juce::var &message) {
  if (!isStandaloneMode) {
    return false;
  }
  const auto action = message["action"].toString ();
  DrillCommand command{DrillEngine::Action::Start, {}};
  if (action == "start") {
    if (!parseDrillConfig (message, command.config)) {
      return false;
    }
  } else if (action == "tolerance") {
    command.action = DrillEngine::Action::Tolerance;
    command.config.tolerance = static_cast<float> (message["tolerance"]);
    if (!message.hasProperty ("tolerance") || !std::isfinite (command.config.tolerance) ||
        command.config.tolerance < constants::params::toleranceMin ||
        command.config.tolerance > constants::params::toleranceMax) {
      return false;
    }
  } else if (action == "pass_threshold") {
    command.action = DrillEngine::Action::PassThreshold;
    command.config.passThreshold = static_cast<double> (message["passThreshold"]);
    if (!message.hasProperty ("passThreshold") || !std::isfinite (command.config.passThreshold) ||
        command.config.passThreshold < constants::drill::passThresholdMin ||
        command.config.passThreshold > constants::drill::passThresholdMax) {
      return false;
    }
  } else if (action == "hold") {
    command.action = DrillEngine::Action::Hold;
  } else if (action == "too_fast") {
    command.action = DrillEngine::Action::TooFast;
  } else if (action == "pause") {
    command.action = DrillEngine::Action::Pause;
  } else if (action == "resume") {
    command.action = DrillEngine::Action::Resume;
  } else if (action == "retry") {
    command.action = DrillEngine::Action::Retry;
  } else if (action == "finish") {
    command.action = DrillEngine::Action::Finish;
  } else if (action == "disconnect") {
    command.action = DrillEngine::Action::Disconnect;
  } else {
    return false;
  }
  {
    std::lock_guard<std::mutex> lock (drillMutex);
    if ((action == "start") == drillSessionRequested) {
      return false;
    }
    const int next = (drillCommandWrite + 1) % static_cast<int> (drillCommands.size ());
    if (next == drillCommandRead) {
      return false;
    }
    drillCommands[static_cast<size_t> (drillCommandWrite)] = command;
    drillCommandWrite = next;
    if (action == "start") {
      drillSessionRequested = true;
    }
    if (action == "finish") {
      drillSessionRequested = false;
    }
  }
  if (action == "start") {
    cancelCalibration ();
    setClickSubdivisionAndGrid (command.config.clickSubdivisionIndex ());
  }
  if (action == "finish") {
    apvts.getParameter ("click_enabled")->setValueNotifyingHost (0);
    auto *bpm = apvts.getParameter ("internal_bpm");
    bpm->setValueNotifyingHost (bpm->convertTo0to1 (static_cast<float> (getDrillSnapshot ().bpm)));
  }
  return true;
}

juce::String MidiGridAnalyzerAudioProcessor::getDrillStateJson () {
  const auto s = getDrillSnapshot ();
  juce::DynamicObject::Ptr obj = new juce::DynamicObject ();
  const char *states[] = {"idle", "waiting", "playing", "paused", "finished"};
  obj->setProperty ("type", "drill");
  obj->setProperty ("history", drillHistory.updateAndCopy (s));
  obj->setProperty ("state", states[static_cast<int> (s.state)]);
  obj->setProperty ("pattern", juce::String (s.config.pattern.data ()));
  obj->setProperty ("bpm", s.bpm);
  obj->setProperty ("best", s.best);
  obj->setProperty ("attempted", s.attempted);
  obj->setProperty ("nextBpm", s.nextBpm);
  obj->setProperty ("floorBpm", s.floorBpm);
  obj->setProperty ("progress", s.progress);
  obj->setProperty ("accuracy", s.accuracy);
  obj->setProperty ("passes", s.passes);
  obj->setProperty ("blocks", s.blocks);
  obj->setProperty ("activeSlot", s.activeSlot);
  obj->setProperty ("beatsRemaining", s.beatsRemaining);
  obj->setProperty ("missing", s.missing);
  obj->setProperty ("wrong", s.wrong);
  obj->setProperty ("late", s.late);
  obj->setProperty ("extras", s.extras);
  obj->setProperty ("automatic", s.automatic);
  obj->setProperty ("noHits", s.noHits);
  obj->setProperty ("limitReached", s.limitReached);
  obj->setProperty ("tolerance", s.toleranceMs);
  obj->setProperty ("toleranceOverride", s.config.tolerance);
  obj->setProperty ("passThreshold", s.config.passThreshold);
  obj->setProperty ("stepBpm", s.config.stepBpm);
  obj->setProperty ("requiredPasses", constants::drill::requiredPasses);
  obj->setProperty ("hasBlock", s.hasBlock);
  obj->setProperty ("failAccuracy", s.failAccuracy);
  obj->setProperty ("failExtras", s.failExtras);
  obj->setProperty ("failSequence", s.failSequence);
  obj->setProperty ("sequenceDetected", s.sequenceDetected);
  obj->setProperty ("sequenceSeen", s.sequenceSeen);
  return juce::JSON::toString (juce::var (obj.get ()));
}

void MidiGridAnalyzerAudioProcessor::setClickSubdivisionAndGrid (int index) {
  if (index < 0 || index > 4) {
    return;
  }
  auto *click = apvts.getParameter ("click_subdivision");
  click->setValueNotifyingHost (click->convertTo0to1 (static_cast<float> (index)));
  // Off and quarter-note clicks retain the independent display-grid choice.
  if (index >= 2) {
    const int gridIndex = index == 2 ? 0 : index == 4 ? 1 : 2;
    auto *grid = apvts.getParameter ("subdivision");
    grid->setValueNotifyingHost (grid->convertTo0to1 (static_cast<float> (gridIndex)));
  }
}
