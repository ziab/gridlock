#include "PluginProcessor.h"

#include "DrumMap.h"
#include "PluginEditor.h"
#include "Timing.h"

#include <algorithm>
#include <cmath>

// ── Parameter layout helpers ──
namespace
{
void addChoice (std::vector<std::unique_ptr<juce::RangedAudioParameter>> &p, const char *id, const char *name,
                juce::StringArray choices, int def)
{
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID{id, 1}, name, choices, def));
}
void addFloat (std::vector<std::unique_ptr<juce::RangedAudioParameter>> &p, const char *id, const char *name,
               juce::NormalisableRange<float> range, float def)
{
    p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID{id, 1}, name, range, def));
}
void addInt (std::vector<std::unique_ptr<juce::RangedAudioParameter>> &p, const char *id, const char *name, int lo,
             int hi, int def)
{
    p.push_back (std::make_unique<juce::AudioParameterInt> (juce::ParameterID{id, 1}, name, lo, hi, def));
}
void addBool (std::vector<std::unique_ptr<juce::RangedAudioParameter>> &p, const char *id, const char *name, bool def)
{
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID{id, 1}, name, def));
}
} // namespace

MidiGridAnalyzerAudioProcessor::MidiGridAnalyzerAudioProcessor ()
    : AudioProcessor (BusesProperties ().withOutput ("Output", juce::AudioChannelSet::stereo (), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout ())
{
#if JUCE_STANDALONE_APPLICATION
    isStandaloneMode = true;
#else
    isStandaloneMode = juce::JUCEApplicationBase::isStandaloneApp ();
#endif

    auto commandLineArgs = juce::JUCEApplicationBase::getCommandLineParameterArray ();
    for (const auto &arg : commandLineArgs)
    {
        if (arg.containsIgnoreCase ("test") || arg.containsIgnoreCase ("demo"))
        {
            if (auto *param = apvts.getParameter ("test_mode"))
                param->setValueNotifyingHost (1.0f);
            break;
        }
    }

    if (isStandaloneMode)
    {
        remoteServer = std::make_unique<RemoteControlServer> (apvts);
        remoteServer->start ();
    }
}

MidiGridAnalyzerAudioProcessor::~MidiGridAnalyzerAudioProcessor ()
{
    if (remoteServer)
        remoteServer->stop ();
}

juce::AudioProcessorValueTreeState::ParameterLayout MidiGridAnalyzerAudioProcessor::createParameterLayout ()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    addChoice (params, "bars_window", "History Length", {"1 Bar", "2 Bars", "4 Bars", "8 Bars"}, 2);
    addChoice (params, "subdivision", "Grid Subdivision", {"1/8", "1/8T", "1/16", "1/16T", "1/32"}, 2);
    addFloat (params, "tolerance_ms", "Timing Tolerance", {5.0f, 40.0f, 0.5f}, 20.0f);
    addFloat (params, "latency_offset_ms", "System Latency Offset", {-500.0f, 500.0f, 1.0f}, 0.0f);
    addInt (params, "min_velocity", "Velocity Noise Floor", 1, 127, 5);
    addFloat (params, "internal_bpm", "Internal BPM", {40.0f, 300.0f, 0.1f}, 120.0f);
    addInt (params, "time_sig_num", "Time Sig Numerator", 2, 12, 4);
    addChoice (params, "click_subdivision", "Click Subdivision", {"Off", "1/4 Notes", "1/8 Notes", "1/16 Notes", "Triplets"}, 1);
    addChoice (params, "click_sample_preset", "Click Sound Preset",
               {"Wood Clave", "Drum Stick Click", "Digital Beep", "Cowbell"}, 0);
    addFloat (params, "click_volume", "Click Volume", {0.0f, 2.0f, 0.01f}, 0.8f);
    addFloat (params, "click_pan", "Click Panning", {-1.0f, 1.0f, 0.05f}, 0.0f);
    addBool (params, "click_enabled", "Metronome On/Off", true);
    addBool (params, "is_paused", "Pause/Freeze Grid", false);
    addBool (params, "show_ms_labels", "Display MS Offsets", true);
    addBool (params, "show_velocity_labels", "Display Velocity", false);
    addBool (params, "show_note_numbers", "Display Note Numbers", false);
    addBool (params, "test_mode", "Rock Beat Demo Mode", false);
    addChoice (params, "note_filter", "Display Mode", {"All Notes", "Roland/GM Drum Map", "Custom"}, 1);

    return {params.begin (), params.end ()};
}

double MidiGridAnalyzerAudioProcessor::getSubdivisionPpq (int index) noexcept
{
    switch (index)
    {
    case 0:  return 0.5;
    case 1:  return 0.5 * (2.0 / 3.0);
    case 2:  return 0.25;
    case 3:  return 0.25 * (2.0 / 3.0);
    case 4:  return 0.125;
    default: return 0.25;
    }
}

MidiGridAnalyzerAudioProcessor::ParamSnapshot
MidiGridAnalyzerAudioProcessor::readSnapshot (const juce::AudioProcessorValueTreeState &state) noexcept
{
    ParamSnapshot s;
    s.subChoice      = static_cast<int> (state.getRawParameterValue ("subdivision")->load ());
    s.toleranceMs    = state.getRawParameterValue ("tolerance_ms")->load ();
    s.userLatencyMs  = state.getRawParameterValue ("latency_offset_ms")->load ();
    s.minVelocity    = static_cast<int> (state.getRawParameterValue ("min_velocity")->load ());
    s.internalBpm    = state.getRawParameterValue ("internal_bpm")->load ();
    s.timeSigNum     = static_cast<int> (state.getRawParameterValue ("time_sig_num")->load ());
    s.clickSubChoice = static_cast<int> (state.getRawParameterValue ("click_subdivision")->load ());
    s.clickPreset    = static_cast<int> (state.getRawParameterValue ("click_sample_preset")->load ());
    s.clickVolume    = state.getRawParameterValue ("click_volume")->load ();
    s.clickPan       = state.getRawParameterValue ("click_pan")->load ();
    s.clickEnabled   = state.getRawParameterValue ("click_enabled")->load () > 0.5f;
    s.isPaused       = state.getRawParameterValue ("is_paused")->load () > 0.5f;
    s.testMode       = state.getRawParameterValue ("test_mode")->load () > 0.5f;
    return s;
}

void MidiGridAnalyzerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    internalPpqPosition = 0.0;
    lastTestBeatTick = -1.0;
    ringBuffer.reset ();
    clickGenerator.prepareToPlay (sampleRate);
}

void MidiGridAnalyzerAudioProcessor::releaseResources () {}

bool MidiGridAnalyzerAudioProcessor::isBusesLayoutSupported (const BusesLayout &layouts) const
{
    if (layouts.getMainOutputChannelSet () != juce::AudioChannelSet::mono ()
        && layouts.getMainOutputChannelSet () != juce::AudioChannelSet::stereo ())
        return false;
    return true;
}

void MidiGridAnalyzerAudioProcessor::processBlock (juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
    buffer.clear ();

    const double sampleRate = getSampleRate ();
    const int numSamples = buffer.getNumSamples ();
    const ParamSnapshot p = readSnapshot (apvts);
    const double gridInterval = getSubdivisionPpq (p.subChoice);

    updateHostSyncAndPlayhead (p.internalBpm, p.timeSigNum, p.isPaused);

    const double srToUse = (sampleRate > 0.0) ? sampleRate : 44100.0;
    const double blockStartPpq = currentPpqPosition;
    const double blockEndPpq = blockStartPpq + numSamples * (currentBpm / 60.0) / srToUse;

    if (isStandaloneMode && !p.isPaused)
        clickGenerator.renderBlock (buffer, numSamples, srToUse, blockStartPpq, currentBpm, currentTimeSigNum,
                                   p.clickSubChoice, p.clickPreset, p.clickVolume, p.clickPan, p.clickEnabled);

    const double autoLatencyMs = (static_cast<double> (getLatencySamples ()) / srToUse) * 1000.0;
    const double totalLatencyMs = autoLatencyMs + static_cast<double> (p.userLatencyMs);
    const double totalLatencyPpq = (totalLatencyMs / 1000.0) * (currentBpm / 60.0);

    if (!p.isPaused)
        processIncomingMidi (midiMessages, srToUse, gridInterval, p.toleranceMs, p.minVelocity, totalLatencyPpq);

    if (p.testMode && !p.isPaused && currentTimeSigNum > 0)
        generateTestModeBeat (blockStartPpq, blockEndPpq, totalLatencyPpq, gridInterval, p.toleranceMs);

    if (!p.isPaused)
        internalPpqPosition += numSamples * (currentBpm / 60.0) / srToUse;
}

void MidiGridAnalyzerAudioProcessor::updateHostSyncAndPlayhead (float internalBpmVal, int timeSigNumVal,
                                                                bool isPausedVal)
{
    bool hostPpqValid = false;
    double hostBpm = internalBpmVal;
    double hostPpq = internalPpqPosition;
    int hostTimeSigNum = timeSigNumVal;
    bool hostPlaying = false;

    if (auto playHead = getPlayHead ())
        if (auto pos = playHead->getPosition ())
        {
            if (auto bpmOpt = pos->getBpm ())
                if (*bpmOpt > 0.0) hostBpm = *bpmOpt;
            if (auto ppqOpt = pos->getPpqPosition ()) { hostPpq = *ppqOpt; hostPpqValid = true; }
            if (auto tsOpt = pos->getTimeSignature ())
                if (tsOpt->numerator > 0) hostTimeSigNum = tsOpt->numerator;
            hostPlaying = pos->getIsPlaying ();
        }

    currentBpm = (hostBpm > 0.0) ? hostBpm : internalBpmVal;
    currentPpqPosition = hostPpqValid ? hostPpq : internalPpqPosition;
    currentTimeSigNum = (hostTimeSigNum > 0) ? hostTimeSigNum : timeSigNumVal;
    hostIsPlaying = (hostPlaying || !hostPpqValid) && !isPausedVal;
}

bool MidiGridAnalyzerAudioProcessor::updateHiHatHistoryAndShouldFilter (uint8_t noteNum, uint8_t velocity, double nowMs)
{
    const bool isOtherHiHat = (noteNum == DrumMap::ClosedHiHatEdge || noteNum == DrumMap::ClosedHiHat
                               || noteNum == DrumMap::PedalHiHat || noteNum == DrumMap::OpenHiHatEdge);

    if (isOtherHiHat)
    {
        lastOtherHiHatTimeMs = nowMs;
        return false;
    }

    if (noteNum == DrumMap::OpenHiHat)
    {
        const double windowMs = DrumMap::hiHatDebounceWindowMs (velocity);
        if ((nowMs - lastOtherHiHatTimeMs) < windowMs)
            return true;
    }

    return false;
}

void MidiGridAnalyzerAudioProcessor::processIncomingMidi (const juce::MidiBuffer &midiMessages, double srToUse,
                                                          double gridInterval, float toleranceMs, int minVelocity,
                                                          double totalLatencyPpq)
{
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage ();
        if (! (msg.isNoteOn () && msg.getVelocity () >= minVelocity
               && !DrumMap::isExcluded (static_cast<uint8_t> (msg.getNoteNumber ()))))
            continue;

        const uint8_t noteNum = static_cast<uint8_t> (msg.getNoteNumber ());
        const uint8_t velocity = static_cast<uint8_t> (msg.getVelocity ());
        const double nowMs = juce::Time::getMillisecondCounterHiRes ();

        if (updateHiHatHistoryAndShouldFilter (noteNum, velocity, nowMs))
            continue;

        const int sampleOffset = metadata.samplePosition;
        const double rawHitPpq = currentPpqPosition + (sampleOffset * (currentBpm / 60.0) / srToUse);
        const double compensatedHitPpq = rawHitPpq - totalLatencyPpq;
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
    }
}

// ── Test-mode helpers ──
double MidiGridAnalyzerAudioProcessor::generateHumanizedDeviationMs (float toleranceMs)
{
    const float roll = random.nextFloat ();
    if (roll < 0.80f)
    {
        const double factor = static_cast<double> (random.nextFloat () * 1.4f - 0.7f);
        return factor * static_cast<double> (toleranceMs);
    }
    if (roll < 0.90f)
    {
        const double factor = static_cast<double> (1.25f + random.nextFloat () * 0.95f);
        return -factor * static_cast<double> (toleranceMs);
    }
    const double factor = static_cast<double> (1.25f + random.nextFloat () * 0.95f);
    return factor * static_cast<double> (toleranceMs);
}

HitEvent MidiGridAnalyzerAudioProcessor::makeQuantizedHit (uint8_t note, uint8_t vel, double targetCompPpq,
                                                           double gridInterval, double bpm, float toleranceMs,
                                                           double totalLatencyPpq) const
{
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
                                                           float toleranceMs)
{
    constexpr double subInterval = 0.5; // 8th notes
    const double firstTick = std::floor (blockStartPpq / subInterval) * subInterval;

    for (double tick = firstTick; tick < blockEndPpq; tick += subInterval)
    {
        if (tick < blockStartPpq || tick <= lastTestBeatTick)
            continue;

        lastTestBeatTick = tick;

        const double barPpq = std::fmod (tick, static_cast<double> (currentTimeSigNum));
        const int beatInBar = static_cast<int> (std::floor (barPpq));
        const double subFraction = std::fmod (tick, 1.0);
        const bool is8thAnd = (std::abs (subFraction - 0.5) < 0.001);

        auto emit = [this, tick, totalLatencyPpq, gridInterval, toleranceMs] (uint8_t note, uint8_t vel)
        {
            const double devMs = generateHumanizedDeviationMs (toleranceMs);
            const double devPpq = (devMs / 1000.0) * (currentBpm / 60.0);
            const double targetCompPpq = tick + devPpq;
            ringBuffer.push (makeQuantizedHit (note, vel, targetCompPpq, gridInterval, currentBpm, toleranceMs,
                                               totalLatencyPpq));
        };

        emit (DrumMap::ClosedHiHat,
              static_cast<uint8_t> (DrumMap::TestModeVelocity::BaseHiHat + random.nextInt (DrumMap::TestModeVelocity::HiHatRange)));

        if (!is8thAnd && (beatInBar == 0 || beatInBar == 2))
            emit (DrumMap::Kick,
                  static_cast<uint8_t> (DrumMap::TestModeVelocity::BaseKick + random.nextInt (DrumMap::TestModeVelocity::KickRange)));

        if (!is8thAnd && (beatInBar == 1 || beatInBar == 3))
            emit (DrumMap::SnareHead,
                  static_cast<uint8_t> (DrumMap::TestModeVelocity::BaseSnare + random.nextInt (DrumMap::TestModeVelocity::SnareRange)));

        if (!is8thAnd && beatInBar == 0
            && std::abs (std::fmod (tick, static_cast<double> (currentTimeSigNum) * 4.0)) < 0.001)
            emit (DrumMap::Crash1, DrumMap::TestModeVelocity::Crash);
    }
}

juce::AudioProcessorEditor *MidiGridAnalyzerAudioProcessor::createEditor ()
{
    return new MidiGridAnalyzerAudioProcessorEditor (*this);
}

void MidiGridAnalyzerAudioProcessor::getStateInformation (juce::MemoryBlock &destData)
{
    auto state = apvts.copyState ();
    std::unique_ptr<juce::XmlElement> xml (state.createXml ());
    copyXmlToBinary (*xml, destData);
}

void MidiGridAnalyzerAudioProcessor::setStateInformation (const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get () != nullptr)
        if (xmlState->hasTagName (apvts.state.getType ()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter ()
{
    return new MidiGridAnalyzerAudioProcessor ();
}
