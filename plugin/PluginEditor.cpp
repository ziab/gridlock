#include "PluginEditor.h"

#include "AsciiTabRenderer.h"
#include "PluginProcessor.h"
#include "Theme.h"

#include <algorithm>
#include <juce_audio_devices/juce_audio_devices.h>

namespace {
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
  cb.addItemList (items, 1);
  addAndMakeVisible (cb);
  label.setText (labelText, juce::dontSendNotification);
  label.attachToComponent (&cb, false);
  label.setFont (juce::Font (11.0f, juce::Font::bold));
  label.setColour (juce::Label::textColourId, Theme::col (Theme::textLabel));
}

void MidiGridAnalyzerAudioProcessorEditor::styleSlider (juce::Slider &s, juce::Label &label, const char *labelText,
                                                        int textBoxW, juce::uint32 labelCol) {
  s.setSliderStyle (juce::Slider::LinearBar);
  s.setTextBoxStyle (juce::Slider::TextBoxLeft, false, textBoxW, 20);
  addAndMakeVisible (s);
  label.setText (labelText, juce::dontSendNotification);
  label.attachToComponent (&s, false);
  label.setFont (juce::Font (11.0f, juce::Font::bold));
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
  juce::Desktop::setScreenSaverEnabled (false);
  setResizable (true, true);

  if (auto *display = juce::Desktop::getInstance ().getDisplays ().getPrimaryDisplay ()) {
    const auto area = display->userArea;
    const int minW = std::min (1150, area.getWidth () / 2);
    const int minH = std::min (480, area.getHeight () / 2);
    const int defaultW = std::min (1640, static_cast<int> (area.getWidth () * 0.92));
    const int defaultH = std::min (680, static_cast<int> (area.getHeight () * 0.8));
    setResizeLimits (minW, minH, area.getWidth (), area.getHeight ());
    setSize (defaultW, defaultH);
  } else {
    setResizeLimits (1150, 480, 1920, 1080);
    setSize (1640, 640);
  }

  addAndMakeVisible (gridComponent);
  setupControls ();
  attachParameters ();
  setupTimeSigHandling ();

  openGLContext.attachTo (*this);
  startTimerHz (60);
}

void MidiGridAnalyzerAudioProcessorEditor::setupControls () {
  styleCombo (barsComboBox, {"1 Bar", "2 Bars", "4 Bars", "8 Bars"}, barsLabel, "Bars:");
  styleCombo (subdivisionComboBox, {"1/8", "1/8T", "1/16", "1/16T", "1/32"}, subdivisionLabel, "Subdiv:");
  styleSlider (toleranceSlider, toleranceLabel, "Tolerance:", 45);
  styleSlider (latencySlider, latencyLabel, "Latency:", 45, Theme::skyBlue);
  // Device latency read-only display next to user latency
  deviceLatencyLabel.setText ("Dev:", juce::dontSendNotification);
  deviceLatencyLabel.setFont (juce::Font (11.0f, juce::Font::bold));
  deviceLatencyLabel.setColour (juce::Label::textColourId, Theme::col (Theme::textLabel));
  deviceLatencyLabel.setJustificationType (juce::Justification::centredLeft);
  deviceLatencyLabel.setTooltip (
      "Audio output latency from device (blockSize + hidden buffers). "
      "Input/MIDI latency ~1-3ms USB jitter not measurable via audio device - use Latency slider to trim. "
      "Shows output latency only; input (MIDI) assumed 0 - calibrate with Latency if needed.");
  addAndMakeVisible (deviceLatencyLabel);
  styleSlider (velocitySlider, velocityLabel, "Min Vel:", 35);
  styleSlider (bpmSlider, bpmLabel, "BPM:", 45);
  styleCombo (timeSigComboBox, {"2/4", "3/4", "4/4", "5/4", "6/8", "7/8"}, timeSigLabel, "Time Sig:");
  styleCombo (clickSubComboBox, {"Off", "1/4 Notes", "1/8 Notes", "1/16 Notes", "Triplets"}, clickSubLabel,
              "Click Sub:");
  styleCombo (clickSoundComboBox, {"Wood Clave", "Drum Stick Click", "Digital Beep", "Cowbell"}, clickSoundLabel,
              "Click Sound:");
  styleSlider (clickVolumeSlider, clickVolLabel, "Click Vol:", 35);
  styleSlider (clickPanSlider, clickPanLabel, "Click Pan:", 35);

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
    pauseButton.setButtonText (pauseButton.getToggleState () ? "RESUME" : "PAUSE");
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
  saveWindowState ();
  openGLContext.detach ();
  juce::Desktop::setScreenSaverEnabled (true);
  stopTimer ();
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

void MidiGridAnalyzerAudioProcessorEditor::saveWindowState () {
  if (!processorRef.isStandaloneAppMode ()) {
    return;
  }
  const bool isMax = isWindowMaximized ();
  // Persist for DAW reload via APVTS child (host saves state)
  auto &state = processorRef.getAPVTS ().state;
  state.getOrCreateChildWithName ("uiState", nullptr).setProperty ("isMaximized", isMax, nullptr);
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
    file->setValue ("isMaximized", isMax);
  }
  lastMaximizedState = isMax;
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
  if (shouldMax) {
    // Defer until peer exists
    juce::MessageManager::callAsync ([this] { setWindowMaximized (true); });
  }
  lastMaximizedState = shouldMax;
  windowStateRestored = true;
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
  const double devMs = processorRef.getDeviceLatencyMs (sr > 0.0 ? sr : 44100.0);
  const int outSamples = processorRef.getDeviceOutputLatencySamples ();
  const int inSamples = processorRef.getDeviceInputLatencySamples ();
  juce::String txt;
  if (outSamples > 0 || inSamples > 0) {
    txt = juce::String (outSamples) + "s " + juce::String (devMs, 1) + "ms";
    if (inSamples > 0) {
      txt += " (+in " + juce::String (inSamples) + "s)";
    }
  } else if (!processorRef.isStandaloneAppMode ()) {
    txt = "n/a (VST)";
  } else {
    txt = "--";
  }
  deviceLatencyLabel.setText ("Dev: " + txt, juce::dontSendNotification);
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
  return s;
}

void MidiGridAnalyzerAudioProcessorEditor::timerCallback () {
  if (windowStateRestored && processorRef.isStandaloneAppMode ()) {
    const bool curMax = isWindowMaximized ();
    if (curMax != lastMaximizedState) {
      saveWindowState ();
    }
  }
  updateDeviceLatency ();
  updateCalibrationUI ();
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
    calibrateButton.setButtonText ("CALIBRATE");
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
    calibrateButton.setButtonText ("REC " + juce::String (pct) + "%");
    calibCountOverlay.setText ("GO!", juce::dontSendNotification);
    calibCountOverlay.setVisible (true);
    calibCountOverlay.toFront (false);
    lastCalibStateSeen = stInt;
    return;
  }
  if (st == MidiGridAnalyzerAudioProcessor::CalibState::Done) {
    // PC is passive — companion shows Apply/Add dialog if online.
    // Only auto-reset when no valid result (no hits / jitter) — valid APPLY stays until companion acts
    calibrateButton.setButtonText ("DONE");
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

void MidiGridAnalyzerAudioProcessorEditor::paint (juce::Graphics &g) {
  g.fillAll (Theme::col (Theme::bgMain));
  g.setColour (Theme::col (Theme::bgHeader));
  g.fillRect (0, 0, getWidth (), 65);
  g.setColour (Theme::col (Theme::border));
  g.drawHorizontalLine (65, 0.0f, static_cast<float> (getWidth ()));
}

void MidiGridAnalyzerAudioProcessorEditor::resized () {
  const int headerH = 68;
  const int topMargin = 24;
  const int h = 26;

  struct Item {
    juce::Component *c;
    int w;
  };
  // Widths mirror previous manual layout; FlexBox makes intent explicit and handles overflow.
  Item items[] = {
      {&barsComboBox, 74},       {&subdivisionComboBox, 74}, {&toleranceSlider, 85},    {&latencySlider, 85},
      {&deviceLatencyLabel, 98}, {&calibrateButton, 86},     {&velocitySlider, 65},     {&bpmSlider, 75},
      {&timeSigComboBox, 64},    {&clickSubComboBox, 85},    {&clickSoundComboBox, 95}, {&clickVolumeSlider, 65},
      {&clickPanSlider, 65},     {&clickToggleButton, 75},   {&pauseButton, 65},        {&showMsButton, 85},
      {&showVelButton, 76},      {&showNoteNumButton, 68},   {&testButton, 85},         {&copyTabButton, 84},
  };

  juce::FlexBox fb;
  fb.flexDirection = juce::FlexBox::Direction::row;
  fb.flexWrap = juce::FlexBox::Wrap::noWrap;
  fb.justifyContent = juce::FlexBox::JustifyContent::flexStart;
  fb.alignItems = juce::FlexBox::AlignItems::center;

  for (auto &it : items) {
    fb.items.add (juce::FlexItem (*it.c).withWidth ((float)it.w).withHeight ((float)h).withMargin ({0, 2, 0, 2}));
  }

  // Clear button pinned to the right edge
  fb.items.add (juce::FlexItem ().withFlex (1.0f)); // spacer
  fb.items.add (juce::FlexItem (clearButton).withWidth (78).withHeight ((float)h).withMargin ({0, 4, 0, 2}));

  auto headerArea = juce::Rectangle<int> (4, topMargin, getWidth () - 8, h);
  fb.performLayout (headerArea);

  gridComponent.setBounds (0, headerH, getWidth (), getHeight () - headerH);
  // Count-in overlay centered over grid (throne-readable, grid-synced)
  calibCountOverlay.setBounds (getWidth () / 2 - 120, headerH + getHeight () / 2 - 80, 240, 120);
}
