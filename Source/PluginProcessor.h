#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "RingBuffer.h"

class MidiGridAnalyzerAudioProcessor  : public juce::AudioProcessor
{
public:
    MidiGridAnalyzerAudioProcessor();
    ~MidiGridAnalyzerAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    RingBuffer<4096>& getRingBuffer() { return ringBuffer; }
    
    double getCurrentPpqPosition() const noexcept { return currentPpqPosition; }
    double getCurrentBpm() const noexcept { return currentBpm; }
    bool isHostPlaying() const noexcept { return hostIsPlaying; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static double getSubdivisionPpq(int index) noexcept;

private:
    juce::AudioProcessorValueTreeState apvts;
    RingBuffer<4096> ringBuffer;

    double internalPpqPosition{ 0.0 };
    double currentPpqPosition{ 0.0 };
    double currentBpm{ 120.0 };
    bool hostIsPlaying{ false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiGridAnalyzerAudioProcessor)
};
