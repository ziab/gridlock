#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <vector>
#include <array>

class ClickGenerator
{
public:
    ClickGenerator();
    ~ClickGenerator() = default;

    void prepareToPlay(double sampleRate);
    void reset();

    void renderBlock(juce::AudioBuffer<float>& outputBuffer,
                     int numSamples,
                     double sampleRate,
                     double blockStartPpq,
                     double bpm,
                     int timeSigNum,
                     int clickSubdivisionIndex,
                     int clickPresetIndex,
                     float clickVolume,
                     float clickPan,
                     bool clickEnabled);

private:
    struct ActiveSample {
        const juce::AudioBuffer<float>* buffer{ nullptr };
        int currentSamplePosition{ 0 };
    };

    struct ClickSet {
        juce::AudioBuffer<float> highClick; // Accent (Beat 1)
        juce::AudioBuffer<float> midClick;  // Regular Beats
        juce::AudioBuffer<float> subClick;  // Subdivisions
    };

    void synthesizeAllPresets(double sampleRate);
    double getClickSubdivisionPpq(int index) const noexcept;

    std::array<ClickSet, 4> presetSamples;
    std::vector<ActiveSample> activeVoiceList;
    juce::Random random;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClickGenerator)
};
