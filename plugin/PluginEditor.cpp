#include "PluginEditor.h"

#include "AsciiTabRenderer.h"
#include "DrillPanel.h"
#include "PluginProcessor.h"
#include "Theme.h"

#include <algorithm>
#include <juce_audio_devices/juce_audio_devices.h>

namespace {
constexpr int kPracticeHeight = 82;
constexpr int kToolbarHeight = 68;
constexpr int kHeaderHeight = kPracticeHeight + kToolbarHeight;
constexpr int kControlHeight = 36;
constexpr int kSettingsWidth = 320;
constexpr int kSettingsContentHeight = 730;
constexpr int kGap = 12;
constexpr int kTimeSigNums[] = {2, 3, 4, 5, 6, 7};
constexpr int kBarsValues[] = {1, 2, 4, 8};
} // namespace

// ── lookup helpers ──
int MidiGridAnalyzerAudioProcessorEditor::indexForTimeSig (int num) noexcept {
  for (int i = 0; i < (int)std::size (kTimeSigNums); ++i) {
    if (kTimeSigNums[i] == num) {
      return i;
    }
  }
  return 2; // 4/4
}
int MidiGridAnalyzerAudioProcessorEditor::timeSigForIndex (int idx) noexcept {
  if (idx >= 0 && idx < (int)std::size (kTimeSigNums)) {
    return kTimeSigNums[idx];
  }
  return 4;
}
int MidiGridAnalyzerAudioProcessorEditor::barsForIndex (int idx) noexcept {
  if (idx >= 0 && idx < (int)std::size (kBarsValues)) {
    return kBarsValues[idx];
  }
  return 4;
}

// ── styling helpers ──
void MidiGridAnalyzerAudioProcessorEditor::styleCombo (juce::ComboBox &cb, juce::StringArray items, juce::Label &label,
                                                       const char *labelText) {
  cb.setName (labelText);
  cb.addItemList (items, 1);
  addAndMakeVisible (cb);
  label.setText (labelText, juce::dontSendNotification);
  label.attachToComponent (&cb, false);
  label.setFont (juce::Font (13.0f));
  label.setColour (juce::Label::textColourId, Theme::col (Theme::textLabel));
}

void MidiGridAnalyzerAudioProcessorEditor::styleSlider (juce::Slider &s, juce::Label &label, const char *labelText,
                                                        int textBoxW, juce::uint32 labelCol) {
  s.setName (labelText);
  s.setSliderStyle (juce::Slider::LinearHorizontal);
  s.setTextBoxStyle (juce::Slider::TextBoxRight, false, juce::jmax (textBoxW, 64), 30);
  addAndMakeVisible (s);
  label.setText (labelText, juce::dontSendNotification);
  label.attachToComponent (&s, false);
  label.setFont (juce::Font (13.0f));
  label.setColour (juce::Label::textColourId, Theme::col (labelCol));
}

void MidiGridAnalyzerAudioProcessorEditor::styleToggle (juce::TextButton &b, juce::uint32 onColour,
                                                        juce::uint32 offText, juce::uint32 onText) {
  b.setClickingTogglesState (true);
  b.setColour (juce::TextButton::buttonColourId, Theme::col (Theme::buttonIdle));
  b.setColour (juce::TextButton::buttonOnColourId, Theme::col (onColour));
  b.setColour (juce::TextButton::textColourOffId, Theme::col (offText));
  b.setColour (juce::TextButton::textColourOnId, Theme::col (onText));
  addAndMakeVisible (b);
}

// ── construction ──
MidiGridAnalyzerAudioProcessorEditor::MidiGridAnalyzerAudioProcessorEditor (MidiGridAnalyzerAudioProcessor &p)
    : AudioProcessorEditor (&p), processorRef (p) {
  setLookAndFeel (&desktopLookAndFeel);
  juce::Desktop::setScreenSaverEnabled (false);
  setResizable (true, true);

  if (auto *display = juce::Desktop::getInstance ().getDisplays ().getPrimaryDisplay ()) {
    const auto area = display->userArea;
    const int minW = std::min (900, area.getWidth ());
    const int minH = std::min (480, area.getHeight () / 2);
    const int defaultW = std::min (1640, static_cast<int> (area.getWidth () * 0.92));
    const int defaultH = std::min (680, static_cast<int> (area.getHeight () * 0.8));
    setResizeLimits (minW, minH, area.getWidth (), area.getHeight ());
    setSize (defaultW, defaultH);
  } else {
    setResizeLimits (900, 480, 1920, 1080);
    setSize (1640, 640);
  }

  addAndMakeVisible (gridComponent);
  setupControls ();
  attachParameters ();
  setupTimeSigHandling ();
  setupDesktopLayout ();
  addAndMakeVisible (drillButton);
  drillButton.setEnabled (processorRef.isStandaloneAppMode ());
  drillButton.onClick = [this] {
    if (drillWindow != nullptr) {
      drillWindow->toFront (true);
      return;
    }
    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (new DrillPanel (processorRef));
    options.dialogTitle = "Grouping Drill";
    options.dialogBackgroundColour = Theme::col (Theme::bgMain);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    drillWindow = options.launchAsync ();
  };
  resized ();

  openGLContext.attachTo (*this);
  startTimerHz (60);
}

void MidiGridAnalyzerAudioProcessorEditor::setupControls () {
  styleCombo (barsComboBox, {"1 Bar", "2 Bars", "4 Bars", "8 Bars"}, barsLabel, "Bars");
  styleCombo (subdivisionComboBox, {"1/8", "1/8T", "1/16", "1/16T", "1/32"}, subdivisionLabel, "Grid subdivision");
  styleSlider (toleranceSlider, toleranceLabel, "Tolerance", 45);
  styleSlider (latencySlider, latencyLabel, "Latency offset", 45, Theme::skyBlue);
  // Device latency read-only display next to user latency
  deviceLatencyLabel.setText ("Output estimate unavailable", juce::dontSendNotification);
  deviceLatencyLabel.setFont (juce::Font (13.0f));
  deviceLatencyLabel.setColour (juce::Label::textColourId, Theme::col (Theme::textLabel));
  deviceLatencyLabel.setJustificationType (juce::Justification::centredLeft);
  deviceLatencyLabel.setTooltip (
      "Estimated output latency from one audio buffer. Driver and MIDI delays are not measured here. "
      "Use calibration or latency offset to compensate for the full setup.");
  addAndMakeVisible (deviceLatencyLabel);
  styleSlider (velocitySlider, velocityLabel, "Minimum velocity", 35);
  styleSlider (bpmSlider, bpmLabel, "TEMPO / BPM", 45);
  styleCombo (timeSigComboBox, {"2/4", "3/4", "4/4", "5/4", "6/8", "7/8"}, timeSigLabel, "Time signature");
  styleCombo (clickSubComboBox, {"Off", "1/4 Notes", "1/8 Notes", "1/16 Notes", "Triplets"}, clickSubLabel,
              "Click subdivision");
  styleCombo (clickSoundComboBox, {"Wood Clave", "Drum Stick Click", "Digital Beep"}, clickSoundLabel, "Sound");
  styleSlider (clickVolumeSlider, clickVolLabel, "Volume", 35);
  styleSlider (clickPanSlider, clickPanLabel, "Pan", 35);

  calibrateButton.setColour (juce::TextButton::buttonColourId, Theme::col (Theme::skyBlue));
  calibrateButton.setColour (juce::TextButton::textColourOffId, Theme::col (0xff0a0c10));
  addAndMakeVisible (calibrateButton);
  calibrateButton.onClick = [this] {
    auto st = processorRef.getCalibrationState ();
    if (st == MidiGridAnalyzerAudioProcessor::CalibState::Idle) {
      processorRef.startCalibration ();
    } else {
      // PC is passive — Done state is handled in companion if online.
      // Allow cancel from PC for CountIn/Recording or Done->Idle reset.
      processorRef.cancelCalibration ();
    }
  };

  // Throne-readable count-in overlay (grid-synced, shows on PC)
  calibCountOverlay.setFont (juce::Font (96.0f, juce::Font::bold));
  calibCountOverlay.setColour (juce::Label::textColourId, Theme::col (Theme::emerald));
  calibCountOverlay.setColour (juce::Label::backgroundColourId, juce::Colour (0xaa0a0c10));
  calibCountOverlay.setJustificationType (juce::Justification::centred);
  calibCountOverlay.setVisible (false);
  calibCountOverlay.setInterceptsMouseClicks (false, false);
  addAndMakeVisible (calibCountOverlay);

  styleToggle (clickToggleButton, Theme::buttonClickOn);
  styleToggle (pauseButton, Theme::buttonPauseOn, 0xffffffff, 0xff000000);
  pauseButton.onStateChange = [this] {
    pauseButton.setButtonText (pauseButton.getToggleState () ? "Resume grid" : "Pause grid");
  };
  styleToggle (showMsButton, Theme::buttonMsOn, 0xff818cf8);
  styleToggle (showVelButton, Theme::buttonVelOn, Theme::textLabel);
  styleToggle (showNoteNumButton, Theme::buttonNoteOn, Theme::textLabel);
  styleToggle (testButton, Theme::buttonTestOn, Theme::buttonTestOn);

  copyTabButton.setColour (juce::TextButton::buttonColourId, Theme::col (Theme::emerald));
  copyTabButton.setColour (juce::TextButton::textColourOffId, Theme::col (0xff0a0c10));
  addAndMakeVisible (copyTabButton);
  copyTabButton.onClick = [this] {
    const int barsVal = barsForIndex (barsComboBox.getSelectedItemIndex ());
    const GridViewState state = buildGridViewState (barsVal);
    AsciiTab::RenderOptions opts; // Auto + wrapping
    auto result = AsciiTab::render (eventHistory, state, opts);
    juce::SystemClipboard::copyTextToClipboard (juce::String (result.text));
    // brief visual feedback
    copyTabButton.setButtonText ("Copied!");
    juce::Timer::callAfterDelay (1200, [this] { copyTabButton.setButtonText ("Copy Tab"); });
  };

  clearButton.setColour (juce::TextButton::buttonColourId, Theme::col (Theme::buttonIdle));
  clearButton.setColour (juce::TextButton::textColourOffId, Theme::col (0xffffffff));
  addAndMakeVisible (clearButton);
  clearButton.onClick = [this] {
    eventHistory.clear ();
    gridComponent.clearEvents ();
  };
}

void MidiGridAnalyzerAudioProcessorEditor::attachParameters () {
  auto &apvts = processorRef.getAPVTS ();
  barsAttachment = std::make_unique<ComboBoxAttachment> (apvts, "bars_window", barsComboBox);
  subdivisionAttachment = std::make_unique<ComboBoxAttachment> (apvts, "subdivision", subdivisionComboBox);
  toleranceAttachment = std::make_unique<SliderAttachment> (apvts, "tolerance_ms", toleranceSlider);
  latencyAttachment = std::make_unique<SliderAttachment> (apvts, "latency_offset_ms", latencySlider);
  velocityAttachment = std::make_unique<SliderAttachment> (apvts, "min_velocity", velocitySlider);
  bpmAttachment = std::make_unique<SliderAttachment> (apvts, "internal_bpm", bpmSlider);
  clickSubAttachment = std::make_unique<ComboBoxAttachment> (apvts, "click_subdivision", clickSubComboBox);
  clickSoundAttachment = std::make_unique<ComboBoxAttachment> (apvts, "click_sample_preset", clickSoundComboBox);
  clickVolumeAttachment = std::make_unique<SliderAttachment> (apvts, "click_volume", clickVolumeSlider);
  clickPanAttachment = std::make_unique<SliderAttachment> (apvts, "click_pan", clickPanSlider);
  clickEnabledAttachment = std::make_unique<ButtonAttachment> (apvts, "click_enabled", clickToggleButton);
  pauseAttachment = std::make_unique<ButtonAttachment> (apvts, "is_paused", pauseButton);
  showMsAttachment = std::make_unique<ButtonAttachment> (apvts, "show_ms_labels", showMsButton);
  showVelAttachment = std::make_unique<ButtonAttachment> (apvts, "show_velocity_labels", showVelButton);
  showNoteNumAttachment = std::make_unique<ButtonAttachment> (apvts, "show_note_numbers", showNoteNumButton);
  testAttachment = std::make_unique<ButtonAttachment> (apvts, "test_mode", testButton);
}

void MidiGridAnalyzerAudioProcessorEditor::setupTimeSigHandling () {
  auto &apvts = processorRef.getAPVTS ();

  timeSigComboBox.onChange = [this, &apvts] {
    const int idx = timeSigComboBox.getSelectedItemIndex ();
    const int num = timeSigForIndex (idx);
    if (auto *param = apvts.getParameter ("time_sig_num")) {
      param->setValueNotifyingHost (param->convertTo0to1 (static_cast<float> (num)));
    }
  };

  const int initialTimeSig = static_cast<int> (apvts.getRawParameterValue ("time_sig_num")->load ());
  timeSigComboBox.setSelectedItemIndex (indexForTimeSig (initialTimeSig), juce::dontSendNotification);
}

MidiGridAnalyzerAudioProcessorEditor::~MidiGridAnalyzerAudioProcessorEditor () {
  delete drillWindow.getComponent ();
  // Don't query peer here — top-level may already be detached (returns false and
  // would overwrite a previously saved maximized=true). Persist last known state.
  if (processorRef.isStandaloneAppMode () && windowStateRestored) {
    persistMaximizedState (lastMaximizedState);
  }
  openGLContext.detach ();
  juce::Desktop::setScreenSaverEnabled (true);
  stopTimer ();
  setLookAndFeel (nullptr);
}

void MidiGridAnalyzerAudioProcessorEditor::parentHierarchyChanged () {
  if (auto *topLevel = getTopLevelComponent ()) {
    if (auto *docWin = dynamic_cast<juce::DocumentWindow *> (topLevel)) {
      docWin->setResizable (true, true);
      docWin->setTitleBarButtonsRequired (juce::DocumentWindow::minimiseButton | juce::DocumentWindow::maximiseButton |
                                              juce::DocumentWindow::closeButton,
                                          false);
    }
  }
  if (!windowStateRestored) {
    restoreWindowState ();
  }
}

bool MidiGridAnalyzerAudioProcessorEditor::isWindowMaximized () const {
  if (auto *top = getTopLevelComponent ()) {
    if (auto *dw = dynamic_cast<juce::DocumentWindow *> (top)) {
      return dw->isFullScreen ();
    }
    if (auto *peer = top->getPeer ()) {
      return peer->isFullScreen ();
    }
  }
  return false;
}

void MidiGridAnalyzerAudioProcessorEditor::setWindowMaximized (bool shouldBeMaximized) {
  if (auto *top = getTopLevelComponent ()) {
    if (auto *dw = dynamic_cast<juce::DocumentWindow *> (top)) {
      dw->setFullScreen (shouldBeMaximized);
      return;
    }
    if (auto *peer = top->getPeer ()) {
      peer->setFullScreen (shouldBeMaximized);
    }
  }
}

void MidiGridAnalyzerAudioProcessorEditor::persistMaximizedState (bool isMaximized) {
  if (!processorRef.isStandaloneAppMode ()) {
    return;
  }
  // Persist for DAW reload via APVTS child (host saves state)
  auto &state = processorRef.getAPVTS ().state;
  state.getOrCreateChildWithName ("uiState", nullptr).setProperty ("isMaximized", isMaximized, nullptr);
  // Persist across standalone restarts via PropertiesFile
  juce::ApplicationProperties props;
  juce::PropertiesFile::Options opts;
  opts.applicationName = "Gridlock";
  opts.folderName = "Gridlock";
  opts.filenameSuffix = "settings";
  opts.storageFormat = juce::PropertiesFile::storeAsXML;
  opts.millisecondsBeforeSaving = 0;
  props.setStorageParameters (opts);
  if (auto *file = props.getUserSettings ()) {
    file->setValue ("isMaximized", isMaximized);
  }
  lastMaximizedState = isMaximized;
}

void MidiGridAnalyzerAudioProcessorEditor::saveWindowState () {
  if (!processorRef.isStandaloneAppMode ()) {
    return;
  }
  // If peer not yet available (startup/shutdown), don't overwrite cached state
  // with a false reading — keep lastMaximizedState.
  if (getTopLevelComponent () == nullptr || getTopLevelComponent ()->getPeer () == nullptr) {
    // Still persist the last known value so APVTS child is initialised
    persistMaximizedState (lastMaximizedState);
    return;
  }
  const bool isMax = isWindowMaximized ();
  persistMaximizedState (isMax);
}

void MidiGridAnalyzerAudioProcessorEditor::restoreWindowState () {
  if (!processorRef.isStandaloneAppMode ()) {
    return;
  }
  bool shouldMax = false;
  bool found = false;
  // Prefer PropertiesFile (survives without host save)
  {
    juce::ApplicationProperties props;
    juce::PropertiesFile::Options opts;
    opts.applicationName = "Gridlock";
    opts.folderName = "Gridlock";
    opts.filenameSuffix = "settings";
    opts.storageFormat = juce::PropertiesFile::storeAsXML;
    props.setStorageParameters (opts);
    if (auto *file = props.getUserSettings ()) {
      if (file->containsKey ("isMaximized")) {
        shouldMax = file->getBoolValue ("isMaximized", false);
        found = true;
      }
    }
  }
  // Fallback to APVTS child (for plugin in DAW)
  if (!found) {
    auto uiState = processorRef.getAPVTS ().state.getChildWithName ("uiState");
    if (uiState.isValid ()) {
      shouldMax = static_cast<bool> (uiState.getProperty ("isMaximized", false));
    }
  }
  lastMaximizedState = shouldMax;
  windowStateRestored = true;
  windowMaximizePending = shouldMax;
  if (shouldMax) {
    // Defer until peer exists; editor parentHierarchyChanged can fire before
    // the DocumentWindow peer is created, so retry with delays.
    auto doMax = [this] {
      if (getTopLevelComponent () != nullptr && !isWindowMaximized ()) {
        setWindowMaximized (true);
      }
    };
    juce::MessageManager::callAsync (doMax);
    juce::Timer::callAfterDelay (100, doMax);
    juce::Timer::callAfterDelay (300, doMax);
    juce::Timer::callAfterDelay (600, doMax);
    // Unblock polling after 1s even if fullscreen never took (e.g. headless)
    juce::Timer::callAfterDelay (1000, [this] { windowMaximizePending = false; });
  }
}

// ── timer ──
void MidiGridAnalyzerAudioProcessorEditor::drainRingBuffer () {
  HitEvent e;
  while (processorRef.getRingBuffer ().pop (e)) {
    eventHistory.push_back (e);
  }
}

void MidiGridAnalyzerAudioProcessorEditor::evictOldEvents (double currentPpq, int barsVal) {
  const double windowPpq = static_cast<double> (barsVal) * 4.0;
  const double minPpqThreshold = currentPpq - windowPpq - 8.0;
  eventHistory.erase (
      std::remove_if (eventHistory.begin (), eventHistory.end (),
                      [minPpqThreshold] (const HitEvent &ev) { return ev.hitPpqPosition < minPpqThreshold; }),
      eventHistory.end ());
}

void MidiGridAnalyzerAudioProcessorEditor::updateDeviceLatency () {
  // Standalone device latency: AudioDeviceManager::getCurrentAudioDevice()->getOutputLatencyInSamples()
  // is the true output latency. JUCE's StandalonePluginHolder owns the manager, but its header
  // pulls in the full standalone filter window (heavy, needs module context). To keep the
  // editor buildable as both VST3 and Standalone without that include, we use a two-tier
  // strategy:
  //   1. If JUCE_STANDALONE_APPLICATION, try to locate the standalone's AudioDeviceManager
  //      via the processor's AudioIODevice callbacks (filled in prepareToPlay) – fallback.
  //   2. Heuristic: output latency ≈ current blockSize (one buffer). This tracks the
  //      user-visible drift when the device block size changes (512→1024 ≈11ms, 2048≈46ms)
  //      which is exactly the +/-30ms reported. The full device latency (2*block+hidden)
  //      is a constant offset absorbed by the user's manual Latency slider.
  //
  // If we can resolve the real device later (via a setDeviceManager hook), replace this
  // heuristic with getOutputLatencyInSamples().
  // MIDI latency note: USB MIDI input latency is NOT reported by AudioDeviceManager.
  // Real e-kit MIDI latency is ~1-3ms avg + jitter up to ~5ms (USB poll, driver, hub).
  // We expose it as 0 and rely on user Latency trim. If we ever measure MIDI loopback,
  // add it to deviceInputLatencySamples and getDeviceLatencyMs() will include it.
  const int blockSize = processorRef.getBlockSize ();
  if (blockSize > 0 && processorRef.isStandaloneAppMode ()) {
    // One buffer of output latency; MIDI input ~0 (see note above).
    processorRef.setDeviceLatencySamples (blockSize, 0);
  } else {
    processorRef.setDeviceLatencySamples (0, 0);
  }
  // Update read-only UI label next to Latency slider
  const double sr = processorRef.getSampleRate ();
  const double devMs = processorRef.getDeviceLatencyMs (sr > 0.0 ? sr : constants::params::sampleRateFallback);
  const int outSamples = processorRef.getDeviceOutputLatencySamples ();
  const int inSamples = processorRef.getDeviceInputLatencySamples ();
  juce::String txt;
  if (outSamples > 0 || inSamples > 0) {
    txt = "Output estimate: " + juce::String (devMs, 1) + " ms";
    if (inSamples > 0) {
      txt += " (+in " + juce::String (inSamples) + "s)";
    }
  } else if (!processorRef.isStandaloneAppMode ()) {
    txt = "Output latency managed by host";
  } else {
    txt = "Output estimate unavailable";
  }
  deviceLatencyLabel.setText (txt, juce::dontSendNotification);
}

GridViewState MidiGridAnalyzerAudioProcessorEditor::buildGridViewState (int barsVal) const {
  GridViewState s;
  s.currentPpq = processorRef.getCurrentPpqPosition ();
  s.numBars = barsVal;
  s.gridSubdivisionPpq =
      MidiGridAnalyzerAudioProcessor::getSubdivisionPpq (subdivisionComboBox.getSelectedItemIndex ());
  // Effective interval is what drummer hears (click if enabled)
  const auto snap = MidiGridAnalyzerAudioProcessor::readSnapshot (processorRef.getAPVTS ());
  s.effectiveInterval = MidiGridAnalyzerAudioProcessor::getEffectiveGridInterval (snap);
  s.timeSigNum = processorRef.getCurrentTimeSigNum ();
  s.showMsLabels = processorRef.getAPVTS ().getRawParameterValue ("show_ms_labels")->load () > 0.5f;
  s.showVelocityLabels = processorRef.getAPVTS ().getRawParameterValue ("show_velocity_labels")->load () > 0.5f;
  s.showNoteNumbers = processorRef.getAPVTS ().getRawParameterValue ("show_note_numbers")->load () > 0.5f;
  s.toleranceMs = processorRef.getAPVTS ().getRawParameterValue ("tolerance_ms")->load ();
  s.latencyOffsetMs = processorRef.getAPVTS ().getRawParameterValue ("latency_offset_ms")->load ();
  const double sr = processorRef.getSampleRate ();
  s.deviceLatencyMs = static_cast<float> (processorRef.getDeviceLatencyMs (sr > 0.0 ? sr : 44100.0));
  s.bpm = static_cast<float> (processorRef.getCurrentBpm ());
  const auto d = processorRef.getDrillSnapshot ();
  if (d.state != DrillEngine::State::Idle && d.state != DrillEngine::State::Finished) {
    s.gridSubdivisionPpq = s.effectiveInterval = d.config.interval;
    s.toleranceMs = static_cast<float> (d.toleranceMs);
    s.latencyOffsetMs = static_cast<float> (d.config.latencyMs);
    s.deviceLatencyMs = 0;
  }
  return s;
}

void MidiGridAnalyzerAudioProcessorEditor::timerCallback () {
  if (windowStateRestored && processorRef.isStandaloneAppMode ()) {
    // Don't poll while the initial maximize is still pending (peer may not
    // have applied fullscreen yet); otherwise we would see curMax=false vs
    // last=true and overwrite the persisted true with false.
    if (getTopLevelComponent () == nullptr || getTopLevelComponent ()->getPeer () == nullptr) {
      // Peer not ready — keep pending, skip save
    } else {
      const bool curMax = isWindowMaximized ();
      if (windowMaximizePending) {
        if (curMax == lastMaximizedState) {
          windowMaximizePending = false;
        } else {
          // Still pending — don't treat transient false as user restore
        }
      } else if (curMax != lastMaximizedState) {
        saveWindowState ();
      }
    }
  }
  updateDeviceLatency ();
  updateCalibrationUI ();
  updateDrillUI ();
  drainRingBuffer ();

  const double currentPpq = processorRef.getCurrentPpqPosition ();
  const int barsVal = barsForIndex (barsComboBox.getSelectedItemIndex ());
  evictOldEvents (currentPpq, barsVal);

  const GridViewState state = buildGridViewState (barsVal);
  gridComponent.update (state, eventHistory);
}

void MidiGridAnalyzerAudioProcessorEditor::updateCalibrationUI () {
  const auto st = processorRef.getCalibrationState ();
  const int stInt = static_cast<int> (st);
  if (st == MidiGridAnalyzerAudioProcessor::CalibState::Idle) {
    calibrateButton.setButtonText ("Calibrate");
    calibrateButton.setEnabled (true);
    calibCountOverlay.setVisible (false);
    lastCalibStateSeen = stInt;
    return;
  }
  if (st == MidiGridAnalyzerAudioProcessor::CalibState::CountIn) {
    const int beats = processorRef.getCalibrationBeatsRemaining ();
    calibrateButton.setButtonText (juce::String (beats) + "...");
    calibCountOverlay.setText (juce::String (beats), juce::dontSendNotification);
    calibCountOverlay.setVisible (true);
    calibCountOverlay.toFront (false);
    lastCalibStateSeen = stInt;
    return;
  }
  if (st == MidiGridAnalyzerAudioProcessor::CalibState::Recording) {
    const double prog = processorRef.getCalibrationProgress ();
    const int pct = static_cast<int> (std::round (prog * 100.0));
    calibrateButton.setButtonText ("Recording " + juce::String (pct) + "%");
    calibCountOverlay.setText ("GO!", juce::dontSendNotification);
    calibCountOverlay.setVisible (true);
    calibCountOverlay.toFront (false);
    lastCalibStateSeen = stInt;
    return;
  }
  if (st == MidiGridAnalyzerAudioProcessor::CalibState::Done) {
    // PC is passive — companion shows Apply/Add dialog if online.
    // Only auto-reset when no valid result (no hits / jitter) — valid APPLY stays until companion acts
    calibrateButton.setButtonText ("Done");
    calibCountOverlay.setVisible (false);
    if (lastCalibStateSeen != stInt) {
      const auto res = processorRef.getCalibrationResult ();
      if (!res.hasResult) {
        juce::Timer::callAfterDelay (3000, [this] {
          if (processorRef.getCalibrationState () == MidiGridAnalyzerAudioProcessor::CalibState::Done) {
            auto r2 = processorRef.getCalibrationResult ();
            if (!r2.hasResult) {
              processorRef.cancelCalibration ();
            }
          }
        });
      }
    }
    lastCalibStateSeen = stInt;
  }
}

void MidiGridAnalyzerAudioProcessorEditor::setupDesktopLayout () {
  auto label = [this] (juce::Label &item, const char *text, float size, juce::uint32 colour) {
    item.setText (text, juce::dontSendNotification);
    item.setFont (juce::Font (size, juce::Font::bold));
    item.setColour (juce::Label::textColourId, Theme::col (colour));
    addAndMakeVisible (item);
  };
  label (brandLabel, "GRIDLOCK", 20.0f, Theme::textPrimary);
  label (practiceLabel, "PRACTICE / MIDI ANALYZER", 10.0f, Theme::textMuted);
  label (settingsTitle, "Settings", 22.0f, Theme::textPrimary);
  label (metronomeTitle, "METRONOME", 12.0f, Theme::textMuted);
  label (timingTitle, "TIMING & CALIBRATION", 12.0f, Theme::textMuted);
  label (inputTitle, "INPUT & DEMO", 12.0f, Theme::textMuted);
  settingsViewport.setViewedComponent (&settingsContent, false);
  settingsViewport.setScrollBarsShown (true, false);
  settingsViewport.setScrollBarThickness (6);
  addChildComponent (settingsViewport);
  for (auto *component : std::initializer_list<juce::Component *>{
           &settingsTitle, &metronomeTitle, &timingTitle, &inputTitle, &clickSubComboBox, &clickSubLabel,
           &clickSoundComboBox, &clickSoundLabel, &clickVolumeSlider, &clickVolLabel, &clickPanSlider, &clickPanLabel,
           &latencySlider, &latencyLabel, &deviceLatencyLabel, &calibrateButton, &velocitySlider, &velocityLabel,
           &testButton}) {
    settingsContent.addAndMakeVisible (component);
  }
  styleToggle (settingsButton, Theme::emerald, Theme::textPrimary, Theme::emerald);
  settingsButton.onClick = [this] {
    resized ();
    repaint ();
  };
  addAndMakeVisible (labelsButton);
  labelsButton.onClick = [this] { showLabelsMenu (); };
  setupBarSelection ();
  for (auto *button : {&showMsButton, &showVelButton, &showNoteNumButton}) {
    button->setVisible (false);
  }
  for (auto *button :
       {&clickToggleButton, &testButton, &copyTabButton, &clearButton, &calibrateButton, &labelsButton}) {
    button->setColour (juce::TextButton::textColourOffId, Theme::col (Theme::textPrimary));
    button->setColour (juce::TextButton::textColourOnId, Theme::col (Theme::emerald));
    button->setColour (juce::TextButton::buttonOnColourId, Theme::col (Theme::emerald));
  }
  pauseButton.setColour (juce::TextButton::textColourOnId, Theme::col (Theme::amber));
  pauseButton.onStateChange ();
  clickToggleButton.onStateChange = [this] {
    clickToggleButton.setButtonText (clickToggleButton.getToggleState () ? "Metronome on" : "Metronome off");
  };
  clickToggleButton.onStateChange ();
  testButton.setButtonText ("Demo beat");
  setupValueDisplays ();
  sendLookAndFeelChange ();
}

void MidiGridAnalyzerAudioProcessorEditor::setupBarSelection () {
  barsComboBox.setVisible (false);
  barsLabel.setVisible (true);
  for (size_t i = 0; i < barButtons.size (); ++i) {
    auto &button = barButtons[i];
    button.setButtonText (juce::String (kBarsValues[i]));
    styleToggle (button, Theme::emerald, Theme::textMuted, Theme::emerald);
    button.setRadioGroupId (1);
    button.onClick = [this, i] { barsComboBox.setSelectedItemIndex ((int)i, juce::sendNotificationSync); };
  }
  barsComboBox.onChange = [this] {
    for (size_t i = 0; i < barButtons.size (); ++i) {
      barButtons[i].setToggleState ((int)i == barsComboBox.getSelectedItemIndex (), juce::dontSendNotification);
    }
  };
  barsComboBox.onChange ();
}

void MidiGridAnalyzerAudioProcessorEditor::setupValueDisplays () {
  bpmSlider.setName ("Tempo");
  bpmSlider.setSliderStyle (juce::Slider::IncDecButtons);
  bpmSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 88, 42);
  bpmSlider.setIncDecButtonsMode (juce::Slider::incDecButtonsDraggable_Vertical);
  bpmSlider.setTooltip ("Type a tempo, use + / -, or drag vertically on the buttons.");
  toleranceSlider.setTextValueSuffix (" ms");
  latencySlider.setTextValueSuffix (" ms");
  latencySlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 82, 30);
  deviceLatencyLabel.setFont (juce::Font (12.0f));
  clickVolumeSlider.textFromValueFunction = [] (double value) {
    return juce::String (juce::roundToInt (value * 100.0)) + "%";
  };
  clickVolumeSlider.valueFromTextFunction = [] (const juce::String &text) { return text.getDoubleValue () / 100.0; };
  clickPanSlider.textFromValueFunction = [] (double value) {
    if (std::abs (value) < 0.001) {
      return juce::String ("Centre");
    }
    return juce::String (juce::roundToInt (std::abs (value) * 100.0)) + (value < 0.0 ? " L" : " R");
  };
  clickPanSlider.valueFromTextFunction = [] (const juce::String &text) {
    const double value = text.getDoubleValue () / 100.0;
    return text.containsIgnoreCase ("L") ? -std::abs (value) : value;
  };
  clickVolumeSlider.updateText ();
  clickPanSlider.updateText ();
}

void MidiGridAnalyzerAudioProcessorEditor::showLabelsMenu () {
  juce::PopupMenu menu;
  menu.addItem (1, "Timing offsets (ms)", true, showMsButton.getToggleState ());
  menu.addItem (2, "Velocity", true, showVelButton.getToggleState ());
  menu.addItem (3, "MIDI note numbers", true, showNoteNumButton.getToggleState ());
  menu.showMenuAsync (juce::PopupMenu::Options ().withTargetComponent (&labelsButton),
                      [safe = juce::Component::SafePointer<MidiGridAnalyzerAudioProcessorEditor> (this)] (int result) {
                        if (safe != nullptr && result > 0) {
                          auto *button = result == 1   ? &safe->showMsButton
                                         : result == 2 ? &safe->showVelButton
                                                       : &safe->showNoteNumButton;
                          button->setToggleState (!button->getToggleState (), juce::sendNotificationSync);
                        }
                      });
}

void MidiGridAnalyzerAudioProcessorEditor::paint (juce::Graphics &g) {
  g.fillAll (Theme::col (Theme::bgMain));
  g.setColour (Theme::col (Theme::bgHeader));
  g.fillRect (0, 0, getWidth (), kPracticeHeight);
  g.setColour (Theme::col (Theme::border));
  g.drawHorizontalLine (kPracticeHeight, 0.0f, (float)getWidth ());
  g.drawHorizontalLine (kHeaderHeight - 1, 0.0f, (float)getWidth ());
  if (settingsButton.getToggleState ()) {
    const int x = getWidth () - kSettingsWidth;
    g.setColour (Theme::col (Theme::bgCard));
    g.fillRect (x, kHeaderHeight, kSettingsWidth, getHeight () - kHeaderHeight);
    g.setColour (Theme::col (Theme::border));
    g.drawVerticalLine (x, (float)kHeaderHeight, (float)getHeight ());
  }
}

void MidiGridAnalyzerAudioProcessorEditor::layoutPracticeControls () {
  brandLabel.setBounds (20, 18, 155, 28);
  practiceLabel.setVisible (false);
  drillButton.setBounds (20, 47, 155, 26);
  bpmSlider.setBounds (190, 30, 142, 42);
  bpmLabel.setBounds (190, 8, 142, 20);
  timeSigComboBox.setBounds (350, 30, 96, kControlHeight);
  timeSigLabel.setBounds (350, 8, 110, 20);
  clickToggleButton.setBounds (466, 30, 138, kControlHeight);
  pauseButton.setBounds (616, 30, 110, kControlHeight);
  settingsButton.setBounds (getWidth () - 124, 30, 104, kControlHeight);
}

void MidiGridAnalyzerAudioProcessorEditor::layoutGridControls () {
  const int y = kPracticeHeight + 26;
  int x = 20;
  barsLabel.setBounds (x, kPracticeHeight + 5, 160, 20);
  for (auto &button : barButtons) {
    button.setBounds (x, y, 38, kControlHeight);
    x += 40;
  }
  x += kGap;
  subdivisionComboBox.setBounds (x, y, 132, kControlHeight);
  subdivisionLabel.setBounds (x, kPracticeHeight + 5, 140, 20);
  x += 132 + kGap;
  toleranceSlider.setBounds (x, y, 190, kControlHeight);
  toleranceLabel.setBounds (x, kPracticeHeight + 5, 190, 20);
  labelsButton.setBounds (x + 190 + kGap, y, 80, kControlHeight);
  clearButton.setBounds (getWidth () - 108, y, 88, kControlHeight);
  copyTabButton.setBounds (getWidth () - 208, y, 88, kControlHeight);
}

void MidiGridAnalyzerAudioProcessorEditor::layoutSettings () {
  const int width = kSettingsWidth - 44;
  settingsContent.setSize (kSettingsWidth - 8, kSettingsContentHeight);
  settingsTitle.setBounds (20, 16, width, 32);
  metronomeTitle.setBounds (20, 66, width, 22);
  auto field = [width] (juce::Component &control, juce::Label &label, int y) {
    control.setBounds (20, y + 22, width, kControlHeight);
    label.setBounds (20, y, width, 20);
  };
  field (clickSoundComboBox, clickSoundLabel, 100);
  field (clickSubComboBox, clickSubLabel, 170);
  field (clickVolumeSlider, clickVolLabel, 240);
  field (clickPanSlider, clickPanLabel, 310);
  timingTitle.setBounds (20, 394, width, 22);
  field (latencySlider, latencyLabel, 428);
  deviceLatencyLabel.setBounds (20, 490, width, 22);
  calibrateButton.setBounds (20, 524, width, kControlHeight);
  inputTitle.setBounds (20, 590, width, 22);
  field (velocitySlider, velocityLabel, 624);
  testButton.setBounds (20, 694, width, kControlHeight);
}

void MidiGridAnalyzerAudioProcessorEditor::resized () {
  layoutPracticeControls ();
  layoutGridControls ();
  const bool settingsOpen = settingsButton.getToggleState ();
  settingsViewport.setVisible (settingsOpen);
  settingsViewport.setBounds (getWidth () - kSettingsWidth, kHeaderHeight, kSettingsWidth,
                              getHeight () - kHeaderHeight);
  layoutSettings ();
  gridComponent.setBounds (0, kHeaderHeight, getWidth () - (settingsOpen ? kSettingsWidth : 0),
                           getHeight () - kHeaderHeight);
  calibCountOverlay.setBounds (gridComponent.getBounds ().withSizeKeepingCentre (240, 120));
}

void MidiGridAnalyzerAudioProcessorEditor::updateDrillUI () {
  const auto s = processorRef.getDrillSnapshot ();
  const bool active = s.state != DrillEngine::State::Idle && s.state != DrillEngine::State::Finished;
  for (auto *control :
       std::array<juce::Component *, 9>{&bpmSlider, &timeSigComboBox, &pauseButton, &clickToggleButton, &latencySlider,
                                        &velocitySlider, &testButton, &calibrateButton, &toleranceSlider}) {
    control->setEnabled (!active);
  }
  if (active) {
    bpmSlider.setValue (s.bpm, juce::dontSendNotification);
    drillButton.setButtonText ("Drill: " + juce::String (s.bpm, 0) + " BPM");
  } else if (wasDrilling) {
    bpmSlider.setValue (processorRef.getAPVTS ().getRawParameterValue ("internal_bpm")->load (),
                        juce::dontSendNotification);
    drillButton.setButtonText ("Grouping Drill");
  }
  wasDrilling = active;
}
