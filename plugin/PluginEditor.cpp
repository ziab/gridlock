#include "PluginEditor.h"

#include "PluginProcessor.h"

#include <algorithm>

MidiGridAnalyzerAudioProcessorEditor::MidiGridAnalyzerAudioProcessorEditor (MidiGridAnalyzerAudioProcessor &p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setResizable (true, true);

    // Dynamic screen-based window dimensions & bounds
    if (auto *display = juce::Desktop::getInstance ().getDisplays ().getPrimaryDisplay ())
    {
        const auto area = display->userArea;
        const int minW = std::min (1150, area.getWidth () / 2);
        const int minH = std::min (480, area.getHeight () / 2);
        const int defaultW = std::min (1640, static_cast<int> (area.getWidth () * 0.92));
        const int defaultH = std::min (680, static_cast<int> (area.getHeight () * 0.8));

        setResizeLimits (minW, minH, area.getWidth (), area.getHeight ());
        setSize (defaultW, defaultH);
    }
    else
    {
        setResizeLimits (1150, 480, 1920, 1080);
        setSize (1640, 640);
    }

    addAndMakeVisible (gridComponent);

    // Setup Header Controls
    barsComboBox.addItemList ({"1 Bar", "2 Bars", "4 Bars", "8 Bars"}, 1);
    addAndMakeVisible (barsComboBox);
    barsLabel.attachToComponent (&barsComboBox, false);
    barsLabel.setFont (juce::Font (11.0f, juce::Font::bold));
    barsLabel.setColour (juce::Label::textColourId, juce::Colour (0xffb0b8c8));

    subdivisionComboBox.addItemList ({"1/8", "1/8T", "1/16", "1/16T", "1/32"}, 1);
    addAndMakeVisible (subdivisionComboBox);
    subdivisionLabel.attachToComponent (&subdivisionComboBox, false);
    subdivisionLabel.setFont (juce::Font (11.0f, juce::Font::bold));
    subdivisionLabel.setColour (juce::Label::textColourId, juce::Colour (0xffb0b8c8));

    toleranceSlider.setSliderStyle (juce::Slider::LinearBar);
    toleranceSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 45, 20);
    addAndMakeVisible (toleranceSlider);
    toleranceLabel.attachToComponent (&toleranceSlider, false);
    toleranceLabel.setFont (juce::Font (11.0f, juce::Font::bold));
    toleranceLabel.setColour (juce::Label::textColourId, juce::Colour (0xffb0b8c8));

    latencySlider.setSliderStyle (juce::Slider::LinearBar);
    latencySlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 45, 20);
    addAndMakeVisible (latencySlider);
    latencyLabel.attachToComponent (&latencySlider, false);
    latencyLabel.setFont (juce::Font (11.0f, juce::Font::bold));
    latencyLabel.setColour (juce::Label::textColourId, juce::Colour (0xff38bdf8));

    velocitySlider.setSliderStyle (juce::Slider::LinearBar);
    velocitySlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 35, 20);
    addAndMakeVisible (velocitySlider);
    velocityLabel.attachToComponent (&velocitySlider, false);
    velocityLabel.setFont (juce::Font (11.0f, juce::Font::bold));
    velocityLabel.setColour (juce::Label::textColourId, juce::Colour (0xffb0b8c8));

    bpmSlider.setSliderStyle (juce::Slider::LinearBar);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 45, 20);
    addAndMakeVisible (bpmSlider);
    bpmLabel.attachToComponent (&bpmSlider, false);
    bpmLabel.setFont (juce::Font (11.0f, juce::Font::bold));
    bpmLabel.setColour (juce::Label::textColourId, juce::Colour (0xffb0b8c8));

    timeSigComboBox.addItemList ({"2/4", "3/4", "4/4", "5/4", "6/8", "7/8"}, 1);
    addAndMakeVisible (timeSigComboBox);
    timeSigLabel.attachToComponent (&timeSigComboBox, false);
    timeSigLabel.setFont (juce::Font (11.0f, juce::Font::bold));
    timeSigLabel.setColour (juce::Label::textColourId, juce::Colour (0xffb0b8c8));

    clickSubComboBox.addItemList ({"Off", "1/4 Notes", "1/8 Notes", "1/16 Notes", "Triplets"}, 1);
    addAndMakeVisible (clickSubComboBox);
    clickSubLabel.attachToComponent (&clickSubComboBox, false);
    clickSubLabel.setFont (juce::Font (11.0f, juce::Font::bold));
    clickSubLabel.setColour (juce::Label::textColourId, juce::Colour (0xffb0b8c8));

    clickSoundComboBox.addItemList ({"Wood Clave", "Drum Stick Click", "Digital Beep", "Cowbell"}, 1);
    addAndMakeVisible (clickSoundComboBox);
    clickSoundLabel.attachToComponent (&clickSoundComboBox, false);
    clickSoundLabel.setFont (juce::Font (11.0f, juce::Font::bold));
    clickSoundLabel.setColour (juce::Label::textColourId, juce::Colour (0xffb0b8c8));

    clickVolumeSlider.setSliderStyle (juce::Slider::LinearBar);
    clickVolumeSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 35, 20);
    addAndMakeVisible (clickVolumeSlider);
    clickVolLabel.attachToComponent (&clickVolumeSlider, false);
    clickVolLabel.setFont (juce::Font (11.0f, juce::Font::bold));
    clickVolLabel.setColour (juce::Label::textColourId, juce::Colour (0xffb0b8c8));

    clickPanSlider.setSliderStyle (juce::Slider::LinearBar);
    clickPanSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 35, 20);
    addAndMakeVisible (clickPanSlider);
    clickPanLabel.attachToComponent (&clickPanSlider, false);
    clickPanLabel.setFont (juce::Font (11.0f, juce::Font::bold));
    clickPanLabel.setColour (juce::Label::textColourId, juce::Colour (0xffb0b8c8));

    clickToggleButton.setClickingTogglesState (true);
    clickToggleButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2d3245));
    clickToggleButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff00c853));
    clickToggleButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffffffff));
    clickToggleButton.setColour (juce::TextButton::textColourOnId, juce::Colour (0xffffffff));
    addAndMakeVisible (clickToggleButton);

    pauseButton.setClickingTogglesState (true);
    pauseButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2d3245));
    pauseButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffeab308)); // Amber Paused
    pauseButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffffffff));
    pauseButton.setColour (juce::TextButton::textColourOnId, juce::Colour (0xff000000));
    addAndMakeVisible (pauseButton);
    pauseButton.onStateChange = [this] ()
    { pauseButton.setButtonText (pauseButton.getToggleState () ? "RESUME" : "PAUSE"); };

    showMsButton.setClickingTogglesState (true);
    showMsButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2d3245));
    showMsButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff0284c7)); // Sky Blue MS
    showMsButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff818cf8));
    showMsButton.setColour (juce::TextButton::textColourOnId, juce::Colour (0xffffffff));
    addAndMakeVisible (showMsButton);

    showVelButton.setClickingTogglesState (true);
    showVelButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2d3245));
    showVelButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffa855f7)); // Purple VEL
    showVelButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffb0b8c8));
    showVelButton.setColour (juce::TextButton::textColourOnId, juce::Colour (0xffffffff));
    addAndMakeVisible (showVelButton);

    showNoteNumButton.setClickingTogglesState (true);
    showNoteNumButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2d3245));
    showNoteNumButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff06b6d4)); // Cyan NOTE #
    showNoteNumButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffb0b8c8));
    showNoteNumButton.setColour (juce::TextButton::textColourOnId, juce::Colour (0xffffffff));
    addAndMakeVisible (showNoteNumButton);

    testButton.setClickingTogglesState (true);
    testButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2d3245));
    testButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffec4899)); // Hot Pink TEST
    testButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffec4899));
    testButton.setColour (juce::TextButton::textColourOnId, juce::Colour (0xffffffff));
    addAndMakeVisible (testButton);

    clearButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2d3245));
    clearButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffffffff));
    addAndMakeVisible (clearButton);
    clearButton.onClick = [this] ()
    {
        eventHistory.clear ();
        gridComponent.clearEvents ();
    };

    // Attach APVTS Parameters
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

    // Custom time signature combo box handler mapping to time_sig_num
    timeSigComboBox.onChange = [this, &apvts] ()
    {
        const int idx = timeSigComboBox.getSelectedItemIndex ();
        int num = 4;
        switch (idx)
        {
        case 0:
            num = 2;
            break;
        case 1:
            num = 3;
            break;
        case 2:
            num = 4;
            break;
        case 3:
            num = 5;
            break;
        case 4:
            num = 6;
            break;
        case 5:
            num = 7;
            break;
        default:
            num = 4;
            break;
        }
        if (auto *param = apvts.getParameter ("time_sig_num"))
        {
            param->setValueNotifyingHost (param->convertTo0to1 (static_cast<float> (num)));
        }
    };

    // Set initial selection for time sig combo box
    const int initialTimeSig = static_cast<int> (apvts.getRawParameterValue ("time_sig_num")->load ());
    switch (initialTimeSig)
    {
    case 2:
        timeSigComboBox.setSelectedItemIndex (0, juce::dontSendNotification);
        break;
    case 3:
        timeSigComboBox.setSelectedItemIndex (1, juce::dontSendNotification);
        break;
    case 4:
        timeSigComboBox.setSelectedItemIndex (2, juce::dontSendNotification);
        break;
    case 5:
        timeSigComboBox.setSelectedItemIndex (3, juce::dontSendNotification);
        break;
    case 6:
        timeSigComboBox.setSelectedItemIndex (4, juce::dontSendNotification);
        break;
    case 7:
        timeSigComboBox.setSelectedItemIndex (5, juce::dontSendNotification);
        break;
    default:
        timeSigComboBox.setSelectedItemIndex (2, juce::dontSendNotification);
        break;
    }

    startTimerHz (60);
}

MidiGridAnalyzerAudioProcessorEditor::~MidiGridAnalyzerAudioProcessorEditor ()
{
    stopTimer ();
}

void MidiGridAnalyzerAudioProcessorEditor::parentHierarchyChanged ()
{
    if (auto *topLevel = getTopLevelComponent ())
    {
        if (auto *docWin = dynamic_cast<juce::DocumentWindow *> (topLevel))
        {
            docWin->setResizable (true, true);
            docWin->setTitleBarButtonsRequired (juce::DocumentWindow::minimiseButton |
                                                    juce::DocumentWindow::maximiseButton |
                                                    juce::DocumentWindow::closeButton,
                                                false);
        }
    }
}

void MidiGridAnalyzerAudioProcessorEditor::timerCallback ()
{
    // Drain FIFO queue into GUI event history
    HitEvent event;
    while (processorRef.getRingBuffer ().pop (event))
    {
        eventHistory.push_back (event);
    }

    // Determine current PPQ & History Window Bounds
    const double currentPpq = processorRef.getCurrentPpqPosition ();
    const int barsIdx = barsComboBox.getSelectedItemIndex ();
    int barsVal = 4;
    switch (barsIdx)
    {
    case 0:
        barsVal = 1;
        break;
    case 1:
        barsVal = 2;
        break;
    case 2:
        barsVal = 4;
        break;
    case 3:
        barsVal = 8;
        break;
    default:
        barsVal = 4;
        break;
    }

    const double windowPpq = static_cast<double> (barsVal) * 4.0;
    const double minPpqThreshold = currentPpq - windowPpq - 8.0;

    // Evict old events outside the rolling window
    eventHistory.erase (std::remove_if (eventHistory.begin (), eventHistory.end (),
                                        [minPpqThreshold] (const HitEvent &e)
                                        { return e.hitPpqPosition < minPpqThreshold; }),
                        eventHistory.end ());

    const int subIdx = subdivisionComboBox.getSelectedItemIndex ();
    const double subdivisionPpq = MidiGridAnalyzerAudioProcessor::getSubdivisionPpq (subIdx);
    const int timeSigNum = processorRef.getCurrentTimeSigNum ();
    const bool showMsVal = (processorRef.getAPVTS ().getRawParameterValue ("show_ms_labels")->load () > 0.5f);
    const bool showVelVal = (processorRef.getAPVTS ().getRawParameterValue ("show_velocity_labels")->load () > 0.5f);
    const bool showNoteNumVal = (processorRef.getAPVTS ().getRawParameterValue ("show_note_numbers")->load () > 0.5f);
    const float toleranceVal = processorRef.getAPVTS ().getRawParameterValue ("tolerance_ms")->load ();
    const float latencyVal = processorRef.getAPVTS ().getRawParameterValue ("latency_offset_ms")->load ();
    const float bpmVal = static_cast<float> (processorRef.getCurrentBpm ());

    gridComponent.updateEvents (eventHistory, currentPpq, barsVal, subdivisionPpq, timeSigNum, showMsVal, showVelVal,
                                showNoteNumVal, toleranceVal, latencyVal, bpmVal);
}

void MidiGridAnalyzerAudioProcessorEditor::paint (juce::Graphics &g)
{
    g.fillAll (juce::Colour (0xff0a0c10));

    // Header Background
    g.setColour (juce::Colour (0xff181b24));
    g.fillRect (0, 0, getWidth (), 65);

    g.setColour (juce::Colour (0xff2d3245));
    g.drawHorizontalLine (65, 0.0f, static_cast<float> (getWidth ()));
}

void MidiGridAnalyzerAudioProcessorEditor::resized ()
{
    const int margin = 6;
    const int topMargin = 24;
    const int controlHeight = 26;
    const int headerHeight = 68;

    int x = margin;

    barsComboBox.setBounds (x, topMargin, 74, controlHeight);
    x += 78;

    subdivisionComboBox.setBounds (x, topMargin, 74, controlHeight);
    x += 78;

    toleranceSlider.setBounds (x, topMargin, 85, controlHeight);
    x += 89;

    latencySlider.setBounds (x, topMargin, 85, controlHeight);
    x += 89;

    velocitySlider.setBounds (x, topMargin, 65, controlHeight);
    x += 69;

    bpmSlider.setBounds (x, topMargin, 75, controlHeight);
    x += 79;

    timeSigComboBox.setBounds (x, topMargin, 64, controlHeight);
    x += 68;

    clickSubComboBox.setBounds (x, topMargin, 85, controlHeight);
    x += 89;

    clickSoundComboBox.setBounds (x, topMargin, 95, controlHeight);
    x += 99;

    clickVolumeSlider.setBounds (x, topMargin, 65, controlHeight);
    x += 69;

    clickPanSlider.setBounds (x, topMargin, 65, controlHeight);
    x += 69;

    clickToggleButton.setBounds (x, topMargin, 75, controlHeight);
    x += 79;

    pauseButton.setBounds (x, topMargin, 65, controlHeight);
    x += 69;

    showMsButton.setBounds (x, topMargin, 85, controlHeight);
    x += 89;

    showVelButton.setBounds (x, topMargin, 76, controlHeight);
    x += 80;

    showNoteNumButton.setBounds (x, topMargin, 68, controlHeight);
    x += 72;

    testButton.setBounds (x, topMargin, 85, controlHeight);

    clearButton.setBounds (getWidth () - 85, topMargin, 78, controlHeight);

    gridComponent.setBounds (0, headerHeight, getWidth (), getHeight () - headerHeight);
}
