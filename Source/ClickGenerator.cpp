#include "ClickGenerator.h"
#include <cmath>
#include <algorithm>

ClickGenerator::ClickGenerator()
{
    synthesizeSamples(44100.0);
}

void ClickGenerator::prepareToPlay(double sampleRate)
{
    if (sampleRate > 0.0) {
        synthesizeSamples(sampleRate);
    }
    reset();
}

void ClickGenerator::reset()
{
    activeVoiceList.clear();
}

void ClickGenerator::synthesizeSamples(double sampleRate)
{
    auto generateSample = [this](juce::AudioBuffer<float>& target, double sr, float freq, float durationMs, float gain) {
        const int numSamples = std::max(1, static_cast<int>(std::ceil(sr * (durationMs / 1000.0))));
        target.setSize(1, numSamples);
        float* channel = target.getWritePointer(0);

        double phase = 0.0;
        const double phaseDelta = (2.0 * juce::MathConstants<double>::pi * freq) / sr;

        for (int i = 0; i < numSamples; ++i)
        {
            const float progress = static_cast<float>(i) / static_cast<float>(numSamples);
            const float env = std::exp(-progress * 10.0f);
            const float noise = (random.nextFloat() * 2.0f - 1.0f) * std::exp(-progress * 40.0f) * 0.25f;
            const float tone = std::sin(phase);
            phase += phaseDelta;

            channel[i] = (tone * 0.75f + noise) * env * gain;
        }
    };

    generateSample(highClickSample, sampleRate, 1600.0f, 18.0f, 1.00f);
    generateSample(midClickSample,  sampleRate, 1050.0f, 14.0f, 0.75f);
    generateSample(subClickSample,  sampleRate, 750.0f,  10.0f, 0.45f);
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
                                 float clickVolume,
                                 bool clickEnabled)
{
    if (numSamples <= 0 || sampleRate <= 0.0)
        return;

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

                const juce::AudioBuffer<float>* targetSample = &subClickSample;
                if (isDownbeat) {
                    targetSample = &highClickSample;
                } else if (isQuarterBeat) {
                    targetSample = &midClickSample;
                }

                activeVoiceList.push_back({ targetSample, -sampleOffset });
            }
        }
    }

    // Render active click voices into output audio buffer
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
                const float sampleVal = srcData[voicePos] * clickVolume;
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    outputBuffer.addSample(ch, sampleIdx, sampleVal);
                }
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
