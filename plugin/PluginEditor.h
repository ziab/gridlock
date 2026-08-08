#pragma once

#include "GridComponent.h"
#include "PluginProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_opengl/juce_opengl.h>

class MidiGridAnalyzerAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
  public:
    explicit MidiGridAnalyzerAudioProcessorEditor (MidiGridAnalyzerAudioProcessor &);
    ~MidiGridAnalyzerAudioProcessorEditor () override;

    void paint (juce::Graphics &) override;
    void resized () override;
    void parentHierarchyChanged () override;

  private:
    void timerCallback () override;

    void setupControls ();
    void attachParameters ();
    void setupTimeSigHandling ();

    void drainRingBuffer ();
    void evictOldEvents (double currentPpq, int barsVal);
    GridViewState buildGridViewState (int barsVal) const;

    static int indexForTimeSig (int num) noexcept;
    static int timeSigForIndex (int idx) noexcept;
    static int barsForIndex (int idx) noexcept;

    void styleCombo (juce::ComboBox &cb, juce::StringArray items, juce::Label &label, const char *labelText);
    void styleSlider (juce::Slider &s, juce::Label &label, const char *labelText, int textBoxW, juce::uint32 labelCol = 0xffb0b8c8);
    void styleToggle (juce::TextButton &b, juce::uint32 onColour, juce::uint32 offText = 0xffffffff, juce::uint32 onText = 0xffffffff);

    MidiGridAnalyzerAudioProcessor &processorRef;

    GridComponent gridComponent;

    juce::ComboBox barsComboBox;
    juce::ComboBox subdivisionComboBox;
    juce::Slider toleranceSlider;
    juce::Slider latencySlider;
    juce::Slider velocitySlider;
    juce::Slider bpmSlider;
    juce::ComboBox timeSigComboBox;
    juce::ComboBox clickSubComboBox;
    juce::ComboBox clickSoundComboBox;
    juce::Slider clickVolumeSlider;
    juce::Slider clickPanSlider;
    juce::TextButton clickToggleButton{"CLICK ON"};
    juce::TextButton pauseButton{"PAUSE"};
    juce::TextButton showMsButton{"MS OFFSETS"};
    juce::TextButton showVelButton{"VELOCITY"};
    juce::TextButton showNoteNumButton{"NOTE #"};
    juce::TextButton testButton{"TEST MODE"};
    juce::TextButton copyTabButton{"Copy Tab"};
    juce::TextButton clearButton{"Clear Grid"};

    juce::Label barsLabel{{}, "Bars:"};
    juce::Label subdivisionLabel{{}, "Subdiv:"};
    juce::Label toleranceLabel{{}, "Tolerance:"};
    juce::Label latencyLabel{{}, "Latency:"};
    juce::Label velocityLabel{{}, "Min Vel:"};
    juce::Label bpmLabel{{}, "BPM:"};
    juce::Label timeSigLabel{{}, "Time Sig:"};
    juce::Label clickSubLabel{{}, "Click Sub:"};
    juce::Label clickSoundLabel{{}, "Click Sound:"};
    juce::Label clickVolLabel{{}, "Click Vol:"};
    juce::Label clickPanLabel{{}, "Click Pan:"};

    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<ComboBoxAttachment> barsAttachment;
    std::unique_ptr<ComboBoxAttachment> subdivisionAttachment;
    std::unique_ptr<SliderAttachment> toleranceAttachment;
    std::unique_ptr<SliderAttachment> latencyAttachment;
    std::unique_ptr<SliderAttachment> velocityAttachment;
    std::unique_ptr<SliderAttachment> bpmAttachment;

    std::unique_ptr<ComboBoxAttachment> timeSigAttachment;
    std::unique_ptr<ComboBoxAttachment> clickSubAttachment;
    std::unique_ptr<ComboBoxAttachment> clickSoundAttachment;
    std::unique_ptr<SliderAttachment> clickVolumeAttachment;
    std::unique_ptr<SliderAttachment> clickPanAttachment;
    std::unique_ptr<ButtonAttachment> clickEnabledAttachment;
    std::unique_ptr<ButtonAttachment> pauseAttachment;
    std::unique_ptr<ButtonAttachment> showMsAttachment;
    std::unique_ptr<ButtonAttachment> showVelAttachment;
    std::unique_ptr<ButtonAttachment> showNoteNumAttachment;
    std::unique_ptr<ButtonAttachment> testAttachment;

    std::vector<HitEvent> eventHistory;

    juce::OpenGLContext openGLContext;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiGridAnalyzerAudioProcessorEditor)
};
