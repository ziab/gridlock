#include "PluginProcessor.h"
#include "PluginEditor.h"

MidiGridAnalyzerAudioProcessorEditor::MidiGridAnalyzerAudioProcessorEditor (MidiGridAnalyzerAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setResizable (true, true);
    setResizeLimits (960, 480, 2560, 1440);
    setSize (1320, 640);

    addAndMakeVisible (gridComponent);

    // Setup Header Controls
    barsComboBox.addItemList ({ "1 Bar", "2 Bars", "4 Bars", "8 Bars" }, 1);
    addAndMakeVisible (barsComboBox);
    barsLabel.attachToComponent (&barsComboBox, false);
    barsLabel.setFont (juce::Font (11.0f, juce::Font::bold));
    barsLabel.setColour (juce::Label::textColourId, juce::Colour (0xffb0b8c8));

    subdivisionComboBox.addItemList ({ "1/8", "1/8T", "1/16", "1/16T", "1/32" }, 1);
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

    timeSigComboBox.addItemList ({ "2/4", "3/4", "4/4", "5/4", "6/8", "7/8" }, 1);
    addAndMakeVisible (timeSigComboBox);
    timeSigLabel.attachToComponent (&timeSigComboBox, false);
    timeSigLabel.setFont (juce::Font (11.0f, juce::Font::bold));
    timeSigLabel.setColour (juce::Label::textColourId, juce::Colour (0xffb0b8c8));

    clickSubComboBox.addItemList ({ "Off", "1/4 Notes", "1/8 Notes", "1/16 Notes", "Triplets" }, 1);
    addAndMakeVisible (clickSubComboBox);
    clickSubLabel.attachToComponent (&clickSubComboBox, false);
    clickSubLabel.setFont (juce::Font (11.0f, juce::Font::bold));
    clickSubLabel.setColour (juce::Label::textColourId, juce::Colour (0xffb0b8c8));

    clickSoundComboBox.addItemList ({ "Woodblock", "Digital Beep", "Cowbell", "Stick Click" }, 1);
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

    fullscreenButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1e293b));
    fullscreenButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff38bdf8));
    addAndMakeVisible (fullscreenButton);
    fullscreenButton.onClick = [this]() {
        if (auto* topLevel = getTopLevelComponent())
        {
            if (auto* window = dynamic_cast<juce::ResizableWindow*>(topLevel))
            {
                const bool isFull = window->isFullScreen();
                window->setFullScreen (!isFull);
                fullscreenButton.setButtonText (isFull ? "FULL SCREEN" : "RESTORE");
            }
        }
    };

    clearButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2d3245));
    clearButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffffffff));
    addAndMakeVisible (clearButton);
    clearButton.onClick = [this]() {
        eventHistory.clear();
        gridComponent.clearEvents();
    };

    // Attach APVTS Parameters
    auto& apvts = processorRef.getAPVTS();
    barsAttachment        = std::make_unique<ComboBoxAttachment>(apvts, "bars_window",         barsComboBox);
    subdivisionAttachment = std::make_unique<ComboBoxAttachment>(apvts, "subdivision",         subdivisionComboBox);
    toleranceAttachment   = std::make_unique<SliderAttachment>  (apvts, "tolerance_ms",        toleranceSlider);
    velocityAttachment    = std::make_unique<SliderAttachment>  (apvts, "min_velocity",        velocitySlider);
    bpmAttachment         = std::make_unique<SliderAttachment>  (apvts, "internal_bpm",        bpmSlider);

    clickSubAttachment    = std::make_unique<ComboBoxAttachment>(apvts, "click_subdivision",   clickSubComboBox);
    clickSoundAttachment  = std::make_unique<ComboBoxAttachment>(apvts, "click_sample_preset", clickSoundComboBox);
    clickVolumeAttachment = std::make_unique<SliderAttachment>  (apvts, "click_volume",        clickVolumeSlider);
    clickPanAttachment    = std::make_unique<SliderAttachment>  (apvts, "click_pan",           clickPanSlider);
    clickEnabledAttachment= std::make_unique<ButtonAttachment>  (apvts, "click_enabled",       clickToggleButton);

    // Custom time signature combo box handler mapping to time_sig_num
    timeSigComboBox.onChange = [this, &apvts]() {
        const int idx = timeSigComboBox.getSelectedItemIndex();
        int num = 4;
        switch (idx) {
            case 0: num = 2; break;
            case 1: num = 3; break;
            case 2: num = 4; break;
            case 3: num = 5; break;
            case 4: num = 6; break;
            case 5: num = 7; break;
            default: num = 4; break;
        }
        if (auto* param = apvts.getParameter("time_sig_num")) {
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(num)));
        }
    };

    // Set initial selection for time sig combo box
    const int initialTimeSig = static_cast<int>(apvts.getRawParameterValue("time_sig_num")->load());
    switch (initialTimeSig) {
        case 2: timeSigComboBox.setSelectedItemIndex(0, juce::dontSendNotification); break;
        case 3: timeSigComboBox.setSelectedItemIndex(1, juce::dontSendNotification); break;
        case 4: timeSigComboBox.setSelectedItemIndex(2, juce::dontSendNotification); break;
        case 5: timeSigComboBox.setSelectedItemIndex(3, juce::dontSendNotification); break;
        case 6: timeSigComboBox.setSelectedItemIndex(4, juce::dontSendNotification); break;
        case 7: timeSigComboBox.setSelectedItemIndex(5, juce::dontSendNotification); break;
        default: timeSigComboBox.setSelectedItemIndex(2, juce::dontSendNotification); break;
    }

    startTimerHz (60);
}

MidiGridAnalyzerAudioProcessorEditor::~MidiGridAnalyzerAudioProcessorEditor()
{
    stopTimer();
}

void MidiGridAnalyzerAudioProcessorEditor::timerCallback()
{
    // Drain FIFO queue into GUI event history
    HitEvent event;
    while (processorRef.getRingBuffer().pop (event))
    {
        eventHistory.push_back (event);
    }

    // Determine current PPQ & History Window Bounds
    const double currentPpq = processorRef.getCurrentPpqPosition();
    const int barsIdx = barsComboBox.getSelectedItemIndex();
    int barsVal = 4;
    switch (barsIdx) {
        case 0: barsVal = 1; break;
        case 1: barsVal = 2; break;
        case 2: barsVal = 4; break;
        case 3: barsVal = 8; break;
        default: barsVal = 4; break;
    }

    const double windowPpq = static_cast<double>(barsVal) * 4.0;
    const double minPpqThreshold = currentPpq - windowPpq - 8.0;

    // Evict old events outside the rolling window
    eventHistory.erase (
        std::remove_if (eventHistory.begin(), eventHistory.end(),
                        [minPpqThreshold](const HitEvent& e) {
                            return e.hitPpqPosition < minPpqThreshold;
                        }),
        eventHistory.end()
    );

    const int subIdx = subdivisionComboBox.getSelectedItemIndex();
    const double subdivisionPpq = MidiGridAnalyzerAudioProcessor::getSubdivisionPpq (subIdx);

    gridComponent.updateEvents (eventHistory, currentPpq, barsVal, subdivisionPpq);
}

void MidiGridAnalyzerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0a0c10));

    // Header Background
    g.setColour (juce::Colour (0xff181b24));
    g.fillRect (0, 0, getWidth(), 65);

    g.setColour (juce::Colour (0xff2d3245));
    g.drawHorizontalLine (65, 0.0f, static_cast<float>(getWidth()));
}

void MidiGridAnalyzerAudioProcessorEditor::resized()
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

    toleranceSlider.setBounds (x, topMargin, 90, controlHeight);
    x += 94;

    velocitySlider.setBounds (x, topMargin, 70, controlHeight);
    x += 74;

    bpmSlider.setBounds (x, topMargin, 80, controlHeight);
    x += 84;

    timeSigComboBox.setBounds (x, topMargin, 64, controlHeight);
    x += 68;

    clickSubComboBox.setBounds (x, topMargin, 90, controlHeight);
    x += 94;

    clickSoundComboBox.setBounds (x, topMargin, 95, controlHeight);
    x += 99;

    clickVolumeSlider.setBounds (x, topMargin, 70, controlHeight);
    x += 74;

    clickPanSlider.setBounds (x, topMargin, 70, controlHeight);
    x += 74;

    clickToggleButton.setBounds (x, topMargin, 75, controlHeight);

    fullscreenButton.setBounds (getWidth() - 195, topMargin, 95, controlHeight);
    clearButton.setBounds (getWidth() - 90, topMargin, 82, controlHeight);

    gridComponent.setBounds (0, headerHeight, getWidth(), getHeight() - headerHeight);
}
