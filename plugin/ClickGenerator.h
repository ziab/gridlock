#pragma once

#include <array>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <vector>

class ClickGenerator {
public:
  ClickGenerator ();
  ~ClickGenerator () = default;

  void prepareToPlay (double sampleRate);
  void reset ();

  void renderBlock (juce::AudioBuffer<float> &outputBuffer, int numSamples, double sampleRate, double blockStartPpq,
                    double bpm, int timeSigNum, int clickSubdivisionIndex, int clickPresetIndex, float clickVolume,
                    float clickPan, bool clickEnabled);

  double getClickSubdivisionPpq (int index) const noexcept;

private:
  struct ActiveSample {
    const juce::AudioBuffer<float> *buffer{nullptr};
    int currentSamplePosition{0};
  };

  struct ClickSet {
    juce::AudioBuffer<float> highClick; // Accent (Beat 1)
    juce::AudioBuffer<float> midClick;  // Regular Beats
    juce::AudioBuffer<float> subClick;  // Subdivisions
  };

  void synthesizeAllPresets (double sampleRate);
  void loadWavPreset (const char *wavData, int dataSize, double sampleRate, ClickSet &set);
  void scheduleClicks (double blockStartPpq, double blockEndPpq, double ppqPerSample, int numSamples,
                       double clickInterval, int timeSigNum, const ClickSet &set);
  void mixActiveVoices (juce::AudioBuffer<float> &outputBuffer, int numSamples, float leftGain, float rightGain);
  static void applySoftClipper (juce::AudioBuffer<float> &buffer, int numSamples) noexcept;

  std::array<ClickSet, 3> presetSamples;
  std::vector<ActiveSample> activeVoiceList;
  juce::Random random;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClickGenerator)
};
