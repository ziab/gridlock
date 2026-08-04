#pragma once

#include "ClickGenerator.h"
#include "HitEvent.h"
#include "RemoteControlServer.h"
#include "RingBuffer.h"

#include <juce_audio_processors/juce_audio_processors.h>

class MidiGridAnalyzerAudioProcessor : public juce::AudioProcessor
{
  public:
    MidiGridAnalyzerAudioProcessor ();
    ~MidiGridAnalyzerAudioProcessor () override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources () override;

    bool isBusesLayoutSupported (const BusesLayout &layouts) const override;

    void processBlock (juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    juce::AudioProcessorEditor *createEditor () override;
    bool hasEditor () const override
    {
        return true;
    }

    const juce::String getName () const override
    {
        return JucePlugin_Name;
    }

    bool acceptsMidi () const override
    {
        return true;
    }
    bool producesMidi () const override
    {
        return true;
    }
    bool isMidiEffect () const override
    {
        return false;
    }
    double getTailLengthSeconds () const override
    {
        return 0.0;
    }

    int getNumPrograms () override
    {
        return 1;
    }
    int getCurrentProgram () override
    {
        return 0;
    }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override
    {
        return {};
    }
    void changeProgramName (int, const juce::String &) override {}

    void getStateInformation (juce::MemoryBlock &destData) override;
    void setStateInformation (const void *data, int sizeInBytes) override;

    RingBuffer<4096> &getRingBuffer () noexcept
    {
        return ringBuffer;
    }
    juce::AudioProcessorValueTreeState &getAPVTS () noexcept
    {
        return apvts;
    }

    double getCurrentPpqPosition () const noexcept
    {
        return currentPpqPosition;
    }
    double getCurrentBpm () const noexcept
    {
        return currentBpm;
    }
    int getCurrentTimeSigNum () const noexcept
    {
        return currentTimeSigNum;
    }
    bool isStandaloneAppMode () const noexcept
    {
        return isStandaloneMode;
    }

    static double getSubdivisionPpq (int index) noexcept;

  private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout ();
    void generateTestModeBeat (double blockStartPpq, double blockEndPpq, double totalLatencyPpq, double gridInterval,
                               float toleranceMs);

    juce::AudioProcessorValueTreeState apvts;
    RingBuffer<4096> ringBuffer;
    ClickGenerator clickGenerator;

    double internalPpqPosition{0.0};
    double currentPpqPosition{0.0};
    double currentBpm{120.0};
    int currentTimeSigNum{4};
    bool hostIsPlaying{false};
    bool isStandaloneMode{false};

    // Remote companion app server (standalone mode only)
    std::unique_ptr<RemoteControlServer> remoteServer;

    double lastTestBeatTick{-1.0};
    double lastOtherHiHatTimeMs{-100000.0};
    juce::Random random;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiGridAnalyzerAudioProcessor)
};
