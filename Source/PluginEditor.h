#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "GridComponent.h"

class MidiGridAnalyzerAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                              private juce::Timer
{
public:
    explicit MidiGridAnalyzerAudioProcessorEditor (MidiGridAnalyzerAudioProcessor&);
    ~MidiGridAnalyzerAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    MidiGridAnalyzerAudioProcessor& processorRef;

    GridComponent gridComponent;

    juce::ComboBox barsComboBox;
    juce::ComboBox subdivisionComboBox;
    juce::Slider toleranceSlider;
    juce::Slider velocitySlider;
    juce::Slider bpmSlider;
    juce::TextButton clearButton{ "Clear Grid" };

    juce::Label barsLabel{ {}, "Bars:" };
    juce::Label subdivisionLabel{ {}, "Subdiv:" };
    juce::Label toleranceLabel{ {}, "Tolerance (ms):" };
    juce::Label velocityLabel{ {}, "Min Vel:" };
    juce::Label bpmLabel{ {}, "BPM:" };

    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    std::unique_ptr<ComboBoxAttachment> barsAttachment;
    std::unique_ptr<ComboBoxAttachment> subdivisionAttachment;
    std::unique_ptr<SliderAttachment> toleranceAttachment;
    std::unique_ptr<SliderAttachment> velocityAttachment;
    std::unique_ptr<SliderAttachment> bpmAttachment;

    std::vector<HitEvent> eventHistory;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiGridAnalyzerAudioProcessorEditor)
};
