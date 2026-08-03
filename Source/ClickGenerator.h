#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <vector>

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
                     float clickVolume,
                     bool clickEnabled);

private:
    struct ActiveSample {
        const juce::AudioBuffer<float>* buffer{ nullptr };
        int currentSamplePosition{ 0 };
    };

    void synthesizeSamples(double sampleRate);
    double getClickSubdivisionPpq(int index) const noexcept;

    juce::AudioBuffer<float> highClickSample; // Beat 1 Downbeat
    juce::AudioBuffer<float> midClickSample;  // Beats 2, 3, 4
    juce::AudioBuffer<float> subClickSample;  // Subdivisions

    std::vector<ActiveSample> activeVoiceList;
    juce::Random random;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClickGenerator)
};
