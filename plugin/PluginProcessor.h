#pragma once

#include "ClickGenerator.h"
#include "HitEvent.h"
#include "RemoteControlServer.h"
#include "RingBuffer.h"

#include <juce_audio_processors/juce_audio_processors.h>

class MidiGridAnalyzerAudioProcessor : public juce::AudioProcessor {
public:
  struct ParamSnapshot {
    int subChoice{2};
    float toleranceMs{20.0f};
    float userLatencyMs{0.0f};
    int minVelocity{5};
    float internalBpm{120.0f};
    int timeSigNum{4};
    int clickSubChoice{1};
    int clickPreset{0};
    float clickVolume{0.8f};
    float clickPan{0.0f};
    bool clickEnabled{true};
    bool isPaused{false};
    bool testMode{false};
  };

  MidiGridAnalyzerAudioProcessor ();
  ~MidiGridAnalyzerAudioProcessor () override;

  void prepareToPlay (double sampleRate, int samplesPerBlock) override;
  void releaseResources () override;

  bool isBusesLayoutSupported (const BusesLayout &layouts) const override;

  void processBlock (juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

  juce::AudioProcessorEditor *createEditor () override;
  bool hasEditor () const override {
    return true;
  }

  const juce::String getName () const override {
    return JucePlugin_Name;
  }

  bool acceptsMidi () const override {
    return true;
  }
  bool producesMidi () const override {
    return true;
  }
  bool isMidiEffect () const override {
    return false;
  }
  double getTailLengthSeconds () const override {
    return 0.0;
  }

  int getNumPrograms () override {
    return 1;
  }
  int getCurrentProgram () override {
    return 0;
  }
  void setCurrentProgram (int) override {}
  const juce::String getProgramName (int) override {
    return {};
  }
  void changeProgramName (int, const juce::String &) override {}

  void getStateInformation (juce::MemoryBlock &destData) override;
  void setStateInformation (const void *data, int sizeInBytes) override;

  RingBuffer<4096> &getRingBuffer () noexcept {
    return ringBuffer;
  }
  juce::AudioProcessorValueTreeState &getAPVTS () noexcept {
    return apvts;
  }

  double getCurrentPpqPosition () const noexcept {
    return currentPpqPosition;
  }
  double getCurrentBpm () const noexcept {
    return currentBpm;
  }
  int getCurrentTimeSigNum () const noexcept {
    return currentTimeSigNum;
  }
  bool isStandaloneAppMode () const noexcept {
    return isStandaloneMode;
  }

  static double getSubdivisionPpq (int index) noexcept;
  static ParamSnapshot readSnapshot (const juce::AudioProcessorValueTreeState &apvts) noexcept;

  juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout ();

private:
  void updateHostSyncAndPlayhead (float internalBpmVal, int timeSigNumVal, bool isPausedVal);
  void processIncomingMidi (const juce::MidiBuffer &midiMessages, double srToUse, double gridInterval,
                            float toleranceMs, int minVelocity, double totalLatencyPpq);
  // Hi-hat ghost suppression: records edge/closed hits and filters spurious open-tip retriggers.
  // Returns true if the note should be filtered (dropped).
  bool updateHiHatHistoryAndShouldFilter (uint8_t noteNum, uint8_t velocity, double nowMs);

  void generateTestModeBeat (double blockStartPpq, double blockEndPpq, double totalLatencyPpq, double gridInterval,
                             float toleranceMs);
  double generateHumanizedDeviationMs (float toleranceMs);
  HitEvent makeQuantizedHit (uint8_t note, uint8_t vel, double targetCompPpq, double gridInterval, double bpm,
                             float toleranceMs, double totalLatencyPpq) const;

  juce::AudioProcessorValueTreeState apvts;
  RingBuffer<4096> ringBuffer;
  ClickGenerator clickGenerator;

  double internalPpqPosition{0.0};
  double currentPpqPosition{0.0};
  double currentBpm{120.0};
  int currentTimeSigNum{4};
  bool hostIsPlaying{false};
  bool isStandaloneMode{false};

  std::unique_ptr<RemoteControlServer> remoteServer;

  double lastTestBeatTick{-1.0};
  double lastOtherHiHatTimeMs{-100000.0};
  juce::Random random;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiGridAnalyzerAudioProcessor)
};
