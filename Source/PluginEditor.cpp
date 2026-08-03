#include "PluginProcessor.h"
#include "PluginEditor.h"

MidiGridAnalyzerAudioProcessorEditor::MidiGridAnalyzerAudioProcessorEditor (MidiGridAnalyzerAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setResizable (true, true);
    setResizeLimits (800, 400, 1920, 1080);
    setSize (960, 540);

    addAndMakeVisible (gridComponent);

    // Setup Header Controls
    barsComboBox.addItemList ({ "1 Bar", "2 Bars", "4 Bars", "8 Bars" }, 1);
    addAndMakeVisible (barsComboBox);
    barsLabel.attachToComponent (&barsComboBox, false);
    barsLabel.setFont (juce::Font (12.0f, juce::Font::bold));
    barsLabel.setColour (juce::Label::textColourId, juce::Colour (0xffb0b8c8));

    subdivisionComboBox.addItemList ({ "1/8", "1/8T", "1/16", "1/16T", "1/32" }, 1);
    addAndMakeVisible (subdivisionComboBox);
    subdivisionLabel.attachToComponent (&subdivisionComboBox, false);
    subdivisionLabel.setFont (juce::Font (12.0f, juce::Font::bold));
    subdivisionLabel.setColour (juce::Label::textColourId, juce::Colour (0xffb0b8c8));

    toleranceSlider.setSliderStyle (juce::Slider::LinearBar);
    toleranceSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 50, 20);
    addAndMakeVisible (toleranceSlider);
    toleranceLabel.attachToComponent (&toleranceSlider, false);
    toleranceLabel.setFont (juce::Font (12.0f, juce::Font::bold));
    toleranceLabel.setColour (juce::Label::textColourId, juce::Colour (0xffb0b8c8));

    velocitySlider.setSliderStyle (juce::Slider::LinearBar);
    velocitySlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 40, 20);
    addAndMakeVisible (velocitySlider);
    velocityLabel.attachToComponent (&velocitySlider, false);
    velocityLabel.setFont (juce::Font (12.0f, juce::Font::bold));
    velocityLabel.setColour (juce::Label::textColourId, juce::Colour (0xffb0b8c8));

    bpmSlider.setSliderStyle (juce::Slider::LinearBar);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 50, 20);
    addAndMakeVisible (bpmSlider);
    bpmLabel.attachToComponent (&bpmSlider, false);
    bpmLabel.setFont (juce::Font (12.0f, juce::Font::bold));
    bpmLabel.setColour (juce::Label::textColourId, juce::Colour (0xffb0b8c8));

    clearButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2d3245));
    clearButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffffffff));
    addAndMakeVisible (clearButton);
    clearButton.onClick = [this]() {
        eventHistory.clear();
        gridComponent.clearEvents();
    };

    // Attach APVTS Parameters
    auto& apvts = processorRef.getAPVTS();
    barsAttachment        = std::make_unique<ComboBoxAttachment>(apvts, "bars_window",  barsComboBox);
    subdivisionAttachment = std::make_unique<ComboBoxAttachment>(apvts, "subdivision",  subdivisionComboBox);
    toleranceAttachment   = std::make_unique<SliderAttachment>  (apvts, "tolerance_ms", toleranceSlider);
    velocityAttachment    = std::make_unique<SliderAttachment>  (apvts, "min_velocity", velocitySlider);
    bpmAttachment         = std::make_unique<SliderAttachment>  (apvts, "internal_bpm", bpmSlider);

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
    const double minPpqThreshold = currentPpq - windowPpq - 8.0; // Keep slight margin before eviction

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
    g.fillRect (0, 0, getWidth(), 60);

    g.setColour (juce::Colour (0xff2d3245));
    g.drawHorizontalLine (60, 0.0f, static_cast<float>(getWidth()));
}

void MidiGridAnalyzerAudioProcessorEditor::resized()
{
    const int margin = 8;
    const int topMargin = 22;
    const int controlHeight = 26;
    const int headerHeight = 65;

    int x = margin;

    barsComboBox.setBounds (x, topMargin, 90, controlHeight);
    x += 100;

    subdivisionComboBox.setBounds (x, topMargin, 90, controlHeight);
    x += 100;

    toleranceSlider.setBounds (x, topMargin, 110, controlHeight);
    x += 120;

    velocitySlider.setBounds (x, topMargin, 90, controlHeight);
    x += 100;

    bpmSlider.setBounds (x, topMargin, 90, controlHeight);
    x += 100;

    clearButton.setBounds (getWidth() - 110, topMargin, 95, controlHeight);

    gridComponent.setBounds (0, headerHeight, getWidth(), getHeight() - headerHeight);
}
