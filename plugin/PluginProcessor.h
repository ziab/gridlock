#pragma once

#include "ClickGenerator.h"
#include "HitEvent.h"
#include "RemoteControlServer.h"
#include "RingBuffer.h"

#include <atomic>
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
  static double getClickSubdivisionPpq (int index) noexcept;
  static ParamSnapshot readSnapshot (const juce::AudioProcessorValueTreeState &apvts) noexcept;
  static double getEffectiveGridInterval (const ParamSnapshot &p) noexcept;

  juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout ();

  // Device latency (audio output/input) reported by Standalone AudioDeviceManager.
  // Stored atomically so audio thread can read without lock; updated from message thread.
  // NOTE: Audio output latency is measurable via getOutputLatencyInSamples() (blockSize +
  // hidden buffers). MIDI input latency (USB e-kit ~1-3ms + jitter) is NOT reported by
  // AudioDeviceManager and is currently stored as 0; calibrate via user Latency slider.
  // If MIDI loopback measurement is added later, put it in inputLatencySamples and
  // getDeviceLatencyMs() will include it in total compensation.
  void setDeviceLatencySamples (int outputLatencySamples, int inputLatencySamples) noexcept;
  int getDeviceOutputLatencySamples () const noexcept {
    return deviceOutputLatencySamples.load ();
  }
  int getDeviceInputLatencySamples () const noexcept {
    return deviceInputLatencySamples.load ();
  }
  double getDeviceLatencyMs (double sampleRate) const noexcept;

  // ── Calibration wizard ──
  enum class CalibState : int { Idle = 0, CountIn = 1, Recording = 2, Done = 3 };
  struct CalibResult {
    double meanMs{0.0};
    double medianMs{0.0};
    double sdMs{0.0};
    int hitCount{0};
    int expectedHits{0};
    bool hasResult{false};
  };
  void startCalibration ();
  void cancelCalibration ();
  void applyCalibrationResult (bool addToExisting = false);
  CalibState getCalibrationState () const noexcept {
    return static_cast<CalibState> (calibState.load ());
  }
  CalibResult getCalibrationResult () const;
  // For UI polling: remaining beats in current phase + progress 0..1
  int getCalibrationBeatsRemaining () const noexcept;
  double getCalibrationProgress () const noexcept;
  juce::String getCalibrationStateJson () const;

private:
  void updateHostSyncAndPlayhead (float internalBpmVal, int timeSigNumVal, bool isPausedVal);
  void processIncomingMidi (const juce::MidiBuffer &midiMessages, double srToUse, double gridInterval,
                            float toleranceMs, int minVelocity, double totalLatencyPpq, double calibLatencyPpq);
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

  std::atomic<int> deviceOutputLatencySamples{0};
  std::atomic<int> deviceInputLatencySamples{0};

  // Calibration state (audio thread writes, UI thread reads)
  std::atomic<int> calibState{0}; // CalibState
  double calibStartPpq{0.0};
  double calibCountInEndPpq{0.0};
  double calibRecStartPpq{0.0};
  double calibRecEndPpq{0.0};
  double calibBpm{120.0};
  double calibGridInterval{0.25};
  int calibTimeSigNum{4};
  int calibExpectedHits{0};
  std::vector<double> calibDeltas;
  std::mutex calibMutex;
  CalibResult calibResult;
  std::atomic<bool> calibNeedsBroadcast{false};

  void updateCalibrationState (double blockStartPpq, double blockEndPpq);
  void finalizeCalibration ();

  double lastTestBeatTick{-1.0};
  double lastOtherHiHatTimeMs{-100000.0};
  juce::Random random;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiGridAnalyzerAudioProcessor)
};
