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

    // Controls
    juce::ComboBox barsComboBox;
    juce::ComboBox subdivisionComboBox;
    juce::Slider toleranceSlider;
    juce::Slider velocitySlider;
    juce::Slider bpmSlider;
    juce::ComboBox timeSigComboBox;
    juce::ComboBox clickSubComboBox;
    juce::ComboBox clickSoundComboBox;
    juce::Slider clickVolumeSlider;
    juce::Slider clickPanSlider;
    juce::TextButton clickToggleButton{ "CLICK ON" };
    juce::TextButton clearButton{ "Clear Grid" };

    // Labels
    juce::Label barsLabel{ {}, "Bars:" };
    juce::Label subdivisionLabel{ {}, "Subdiv:" };
    juce::Label toleranceLabel{ {}, "Tolerance:" };
    juce::Label velocityLabel{ {}, "Min Vel:" };
    juce::Label bpmLabel{ {}, "BPM:" };
    juce::Label timeSigLabel{ {}, "Time Sig:" };
    juce::Label clickSubLabel{ {}, "Click Sub:" };
    juce::Label clickSoundLabel{ {}, "Click Sound:" };
    juce::Label clickVolLabel{ {}, "Click Vol:" };
    juce::Label clickPanLabel{ {}, "Click Pan:" };

    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<ComboBoxAttachment> barsAttachment;
    std::unique_ptr<ComboBoxAttachment> subdivisionAttachment;
    std::unique_ptr<SliderAttachment> toleranceAttachment;
    std::unique_ptr<SliderAttachment> velocityAttachment;
    std::unique_ptr<SliderAttachment> bpmAttachment;

    std::unique_ptr<ComboBoxAttachment> timeSigAttachment;
    std::unique_ptr<ComboBoxAttachment> clickSubAttachment;
    std::unique_ptr<ComboBoxAttachment> clickSoundAttachment;
    std::unique_ptr<SliderAttachment> clickVolumeAttachment;
    std::unique_ptr<SliderAttachment> clickPanAttachment;
    std::unique_ptr<ButtonAttachment> clickEnabledAttachment;

    std::vector<HitEvent> eventHistory;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiGridAnalyzerAudioProcessorEditor)
};
