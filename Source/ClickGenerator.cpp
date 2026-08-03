#include "ClickGenerator.h"
#include "BinaryData.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cmath>
#include <algorithm>

ClickGenerator::ClickGenerator()
{
    synthesizeAllPresets(44100.0);
}

void ClickGenerator::prepareToPlay(double sampleRate)
{
    if (sampleRate > 0.0) {
        synthesizeAllPresets(sampleRate);
    }
    reset();
}

void ClickGenerator::reset()
{
    activeVoiceList.clear();
}

void ClickGenerator::loadWavPreset(const char* wavData, int dataSize, double sampleRate, ClickSet& set)
{
    if (wavData == nullptr || dataSize <= 0) return;

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    auto stream = std::make_unique<juce::MemoryInputStream>(wavData, static_cast<size_t>(dataSize), false);
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (std::move (stream)));

    if (reader != nullptr && reader->lengthInSamples > 0)
    {
        juce::AudioBuffer<float> monoBuffer(1, static_cast<int>(reader->lengthInSamples));
        reader->read(&monoBuffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

        const double targetSr = (sampleRate > 0.0) ? sampleRate : 44100.0;
        if (reader->sampleRate > 0.0 && std::abs(reader->sampleRate - targetSr) > 1.0)
        {
            const double ratio = reader->sampleRate / targetSr;
            const int newLen = std::max(1, static_cast<int>(std::ceil(reader->lengthInSamples / ratio)));

            set.midClick.setSize(1, newLen);
            juce::LagrangeInterpolator interpolator;
            interpolator.process(ratio, monoBuffer.getReadPointer(0), set.midClick.getWritePointer(0), newLen);
        }
        else
        {
            set.midClick.setSize(1, monoBuffer.getNumSamples());
            set.midClick.copyFrom(0, 0, monoBuffer, 0, 0, monoBuffer.getNumSamples());
        }

        // High Click: Beat 1 accent (slightly boosted gain)
        set.highClick.setSize(1, set.midClick.getNumSamples());
        set.highClick.copyFrom(0, 0, set.midClick, 0, 0, set.midClick.getNumSamples());
        set.highClick.applyGain(1.3f);

        // Sub Click: Subdivisions (subtler gain)
        set.subClick.setSize(1, set.midClick.getNumSamples());
        set.subClick.copyFrom(0, 0, set.midClick, 0, 0, set.midClick.getNumSamples());
        set.subClick.applyGain(0.55f);
    }
}

void ClickGenerator::synthesizeAllPresets(double sampleRate)
{
    const double sr = (sampleRate > 0.0) ? sampleRate : 44100.0;

    // Load embedded WAV presets
    loadWavPreset(BinaryData::wood_clave_wav, BinaryData::wood_clave_wavSize, sr, presetSamples[0]);
    loadWavPreset(BinaryData::Drum_Stick_Click_01_wav, BinaryData::Drum_Stick_Click_01_wavSize, sr, presetSamples[1]);

    auto makeToneBuffer = [this, sr](juce::AudioBuffer<float>& target, float freq, float durationMs, float gain, float noiseAmt, float decayExponent) {
        const int numSamples = std::max(1, static_cast<int>(std::ceil(sr * (durationMs / 1000.0))));
        target.setSize(1, numSamples);
        float* channel = target.getWritePointer(0);

        double phase = 0.0;
        const double phaseDelta = (2.0 * juce::MathConstants<double>::pi * freq) / sr;

        for (int i = 0; i < numSamples; ++i)
        {
            const float progress = static_cast<float>(i) / static_cast<float>(numSamples);
            const float env = std::exp(-progress * decayExponent);
            const float noise = (random.nextFloat() * 2.0f - 1.0f) * std::exp(-progress * 30.0f) * noiseAmt;
            const float tone = std::sin(phase);
            phase += phaseDelta;

            channel[i] = (tone * (1.0f - noiseAmt) + noise) * env * gain;
        }
    };

    auto makeDualToneBuffer = [this, sr](juce::AudioBuffer<float>& target, float freq1, float freq2, float durationMs, float gain) {
        const int numSamples = std::max(1, static_cast<int>(std::ceil(sr * (durationMs / 1000.0))));
        target.setSize(1, numSamples);
        float* channel = target.getWritePointer(0);

        double phase1 = 0.0, phase2 = 0.0;
        const double phaseDelta1 = (2.0 * juce::MathConstants<double>::pi * freq1) / sr;
        const double phaseDelta2 = (2.0 * juce::MathConstants<double>::pi * freq2) / sr;

        for (int i = 0; i < numSamples; ++i)
        {
            const float progress = static_cast<float>(i) / static_cast<float>(numSamples);
            const float env = std::exp(-progress * 12.0f);
            const float tone = (std::sin(phase1) + 0.6f * std::sin(phase2)) * 0.625f;
            phase1 += phaseDelta1;
            phase2 += phaseDelta2;

            channel[i] = tone * env * gain;
        }
    };

    // Preset 2: Digital Beep
    makeToneBuffer(presetSamples[2].highClick, 1600.0f, 15.0f, 1.00f, 0.00f, 6.0f);
    makeToneBuffer(presetSamples[2].midClick,  800.0f,  12.0f, 0.75f, 0.00f, 6.0f);
    makeToneBuffer(presetSamples[2].subClick,  600.0f,  8.0f,  0.45f, 0.00f, 8.0f);

    // Preset 3: Cowbell
    makeDualToneBuffer(presetSamples[3].highClick, 800.0f, 540.0f, 25.0f, 1.00f);
    makeDualToneBuffer(presetSamples[3].midClick,  560.0f, 380.0f, 20.0f, 0.75f);
    makeDualToneBuffer(presetSamples[3].subClick,  440.0f, 300.0f, 12.0f, 0.45f);
}

double ClickGenerator::getClickSubdivisionPpq(int index) const noexcept
{
    switch (index) {
        case 1: return 1.0;                // 1/4 Notes
        case 2: return 0.5;                // 1/8 Notes
        case 3: return 0.25;               // 1/16 Notes
        case 4: return 1.0 / 3.0;          // Triplets
        default: return 0.0;               // Off
    }
}

void ClickGenerator::renderBlock(juce::AudioBuffer<float>& outputBuffer,
                                 int numSamples,
                                 double sampleRate,
                                 double blockStartPpq,
                                 double bpm,
                                 int timeSigNum,
                                 int clickSubdivisionIndex,
                                 int clickPresetIndex,
                                 float clickVolume,
                                 float clickPan,
                                 bool clickEnabled)
{
    if (numSamples <= 0 || sampleRate <= 0.0)
        return;

    const int presetIdx = std::clamp(clickPresetIndex, 0, static_cast<int>(presetSamples.size()) - 1);
    const auto& currentClickSet = presetSamples[static_cast<size_t>(presetIdx)];

    const int numChannels = outputBuffer.getNumChannels();
    const double ppqPerSample = (bpm / 60.0) / sampleRate;
    const double blockEndPpq = blockStartPpq + numSamples * ppqPerSample;
    const double clickInterval = getClickSubdivisionPpq(clickSubdivisionIndex);

    // Trigger new clicks if enabled
    if (clickEnabled && clickInterval > 0.0 && timeSigNum > 0)
    {
        const double firstTick = std::floor(blockStartPpq / clickInterval) * clickInterval;

        for (double tick = firstTick; tick < blockEndPpq; tick += clickInterval)
        {
            if (tick >= blockStartPpq)
            {
                const int sampleOffset = std::clamp(
                    static_cast<int>(std::round((tick - blockStartPpq) / ppqPerSample)),
                    0, numSamples - 1
                );

                const double barPpq = std::fmod(tick, static_cast<double>(timeSigNum));
                const bool isDownbeat = (std::abs(barPpq) < 0.001 || std::abs(barPpq - timeSigNum) < 0.001);
                const double quarterPpq = std::fmod(tick, 1.0);
                const bool isQuarterBeat = (std::abs(quarterPpq) < 0.001 || std::abs(quarterPpq - 1.0) < 0.001);

                const juce::AudioBuffer<float>* targetSample = &currentClickSet.subClick;
                if (isDownbeat) {
                    targetSample = &currentClickSet.highClick;
                } else if (isQuarterBeat) {
                    targetSample = &currentClickSet.midClick;
                }

                if (targetSample->getNumSamples() > 0)
                {
                    activeVoiceList.push_back({ targetSample, -sampleOffset });
                }
            }
        }
    }

    // Equal-Power Stereo Panning Law
    const float panNorm = std::clamp(clickPan, -1.0f, 1.0f);
    const float angle = (panNorm + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
    const float leftGain = clickVolume * std::cos(angle);
    const float rightGain = clickVolume * std::sin(angle);

    // Render active click voices into output audio buffer with stereo panning
    for (auto it = activeVoiceList.begin(); it != activeVoiceList.end(); )
    {
        const auto* srcBuffer = it->buffer;
        const int srcLength = srcBuffer->getNumSamples();
        const float* srcData = srcBuffer->getReadPointer(0);

        int sampleIdx = 0;
        int voicePos = it->currentSamplePosition;

        while (sampleIdx < numSamples && voicePos < srcLength)
        {
            if (voicePos >= 0)
            {
                const float sampleVal = srcData[voicePos];
                if (numChannels >= 1)
                    outputBuffer.addSample(0, sampleIdx, sampleVal * leftGain);
                if (numChannels >= 2)
                    outputBuffer.addSample(1, sampleIdx, sampleVal * rightGain);
            }
            sampleIdx++;
            voicePos++;
        }

        it->currentSamplePosition = voicePos;

        if (it->currentSamplePosition >= srcLength) {
            it = activeVoiceList.erase(it);
        } else {
            ++it;
        }
    }
}
