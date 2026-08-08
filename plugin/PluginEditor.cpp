#include "PluginEditor.h"

#include "AsciiTabRenderer.h"
#include "PluginProcessor.h"
#include "Theme.h"

#include <algorithm>

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
  styleSlider (velocitySlider, velocityLabel, "Min Vel:", 35);
  styleSlider (bpmSlider, bpmLabel, "BPM:", 45);
  styleCombo (timeSigComboBox, {"2/4", "3/4", "4/4", "5/4", "6/8", "7/8"}, timeSigLabel, "Time Sig:");
  styleCombo (clickSubComboBox, {"Off", "1/4 Notes", "1/8 Notes", "1/16 Notes", "Triplets"}, clickSubLabel,
              "Click Sub:");
  styleCombo (clickSoundComboBox, {"Wood Clave", "Drum Stick Click", "Digital Beep", "Cowbell"}, clickSoundLabel,
              "Click Sound:");
  styleSlider (clickVolumeSlider, clickVolLabel, "Click Vol:", 35);
  styleSlider (clickPanSlider, clickPanLabel, "Click Pan:", 35);

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

GridViewState MidiGridAnalyzerAudioProcessorEditor::buildGridViewState (int barsVal) const {
  GridViewState s;
  s.currentPpq = processorRef.getCurrentPpqPosition ();
  s.numBars = barsVal;
  s.gridSubdivisionPpq =
      MidiGridAnalyzerAudioProcessor::getSubdivisionPpq (subdivisionComboBox.getSelectedItemIndex ());
  s.timeSigNum = processorRef.getCurrentTimeSigNum ();
  s.showMsLabels = processorRef.getAPVTS ().getRawParameterValue ("show_ms_labels")->load () > 0.5f;
  s.showVelocityLabels = processorRef.getAPVTS ().getRawParameterValue ("show_velocity_labels")->load () > 0.5f;
  s.showNoteNumbers = processorRef.getAPVTS ().getRawParameterValue ("show_note_numbers")->load () > 0.5f;
  s.toleranceMs = processorRef.getAPVTS ().getRawParameterValue ("tolerance_ms")->load ();
  s.latencyOffsetMs = processorRef.getAPVTS ().getRawParameterValue ("latency_offset_ms")->load ();
  s.bpm = static_cast<float> (processorRef.getCurrentBpm ());
  return s;
}

void MidiGridAnalyzerAudioProcessorEditor::timerCallback () {
  drainRingBuffer ();

  const double currentPpq = processorRef.getCurrentPpqPosition ();
  const int barsVal = barsForIndex (barsComboBox.getSelectedItemIndex ());
  evictOldEvents (currentPpq, barsVal);

  const GridViewState state = buildGridViewState (barsVal);
  gridComponent.update (state, eventHistory);
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
      {&barsComboBox, 74},       {&subdivisionComboBox, 74}, {&toleranceSlider, 85}, {&latencySlider, 85},
      {&velocitySlider, 65},     {&bpmSlider, 75},           {&timeSigComboBox, 64}, {&clickSubComboBox, 85},
      {&clickSoundComboBox, 95}, {&clickVolumeSlider, 65},   {&clickPanSlider, 65},  {&clickToggleButton, 75},
      {&pauseButton, 65},        {&showMsButton, 85},        {&showVelButton, 76},   {&showNoteNumButton, 68},
      {&testButton, 85},         {&copyTabButton, 84},
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
}
