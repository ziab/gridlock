#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

MidiGridAnalyzerAudioProcessor::MidiGridAnalyzerAudioProcessor()
    : AudioProcessor (BusesProperties()),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
}

MidiGridAnalyzerAudioProcessor::~MidiGridAnalyzerAudioProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout MidiGridAnalyzerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "bars_window", 1 },
        "History Length",
        juce::StringArray{ "1 Bar", "2 Bars", "4 Bars", "8 Bars" },
        2 // Default 4 Bars
    ));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "subdivision", 1 },
        "Grid Subdivision",
        juce::StringArray{ "1/8", "1/8T", "1/16", "1/16T", "1/32" },
        2 // Default 1/16
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "tolerance_ms", 1 },
        "Timing Tolerance",
        juce::NormalisableRange<float>(2.0f, 30.0f, 0.5f),
        10.0f
    ));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ "min_velocity", 1 },
        "Velocity Noise Floor",
        1, 127, 5
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "internal_bpm", 1 },
        "Internal BPM",
        juce::NormalisableRange<float>(40.0f, 300.0f, 0.1f),
        120.0f
    ));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{ "note_filter", 1 },
        "Display Mode",
        juce::StringArray{ "All Notes", "Roland/GM Drum Map", "Custom" },
        1 // Default Roland/GM Drum Map
    ));

    return { params.begin(), params.end() };
}

double MidiGridAnalyzerAudioProcessor::getSubdivisionPpq(int index) noexcept
{
    switch (index) {
        case 0: return 0.5;                  // 1/8
        case 1: return 0.5 * (2.0 / 3.0);    // 1/8T
        case 2: return 0.25;                 // 1/16
        case 3: return 0.25 * (2.0 / 3.0);   // 1/16T
        case 4: return 0.125;                // 1/32
        default: return 0.25;
    }
}

void MidiGridAnalyzerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    internalPpqPosition = 0.0;
    ringBuffer.reset();
}

void MidiGridAnalyzerAudioProcessor::releaseResources()
{
}

bool MidiGridAnalyzerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    juce::ignoreUnused (layouts);
    return true;
}

void MidiGridAnalyzerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    buffer.clear(); // Pure MIDI plugin, clear audio buffer

    const double sampleRate = getSampleRate();
    const int numSamples = buffer.getNumSamples();

    // Read parameters safely
    const int subChoice = static_cast<int>(apvts.getRawParameterValue("subdivision")->load());
    const float toleranceMs = apvts.getRawParameterValue("tolerance_ms")->load();
    const int minVelocity = static_cast<int>(apvts.getRawParameterValue("min_velocity")->load());
    const float internalBpmVal = apvts.getRawParameterValue("internal_bpm")->load();

    const double gridInterval = getSubdivisionPpq(subChoice);

    // Sync with host playhead or fallback to internal clock
    bool hostPpqValid = false;
    double hostBpm = internalBpmVal;
    double hostPpq = internalPpqPosition;
    bool hostPlaying = false;

    if (auto playHead = getPlayHead())
    {
        if (auto pos = playHead->getPosition())
        {
            if (auto bpmOpt = pos->getBpm())
                if (*bpmOpt > 0.0) hostBpm = *bpmOpt;

            if (auto ppqOpt = pos->getPpqPosition())
            {
                hostPpq = *ppqOpt;
                hostPpqValid = true;
            }
            hostPlaying = pos->getIsPlaying();
        }
    }

    currentBpm = (hostBpm > 0.0) ? hostBpm : internalBpmVal;
    currentPpqPosition = hostPpqValid ? hostPpq : internalPpqPosition;
    hostIsPlaying = hostPlaying || !hostPpqValid; // In standalone / fallback, treat as active

    const double srToUse = (sampleRate > 0.0) ? sampleRate : 44100.0;

    // Process incoming MIDI events
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();

        if (msg.isNoteOn() && msg.getVelocity() >= minVelocity)
        {
            const int sampleOffset = metadata.samplePosition;
            const double hitPpq = currentPpqPosition + (sampleOffset * (currentBpm / 60.0) / srToUse);

            const double nearestGridPpq = std::round(hitPpq / gridInterval) * gridInterval;
            const double deltaPpq = hitPpq - nearestGridPpq;
            const double deltaMs = (deltaPpq / (currentBpm / 60.0)) * 1000.0;

            TimingState state = TimingState::OnGrid;
            if (std::abs(deltaMs) <= static_cast<double>(toleranceMs)) {
                state = TimingState::OnGrid;
            } else if (deltaMs < -static_cast<double>(toleranceMs)) {
                state = TimingState::Rush;
            } else {
                state = TimingState::Drag;
            }

            HitEvent event;
            event.noteNumber = static_cast<uint8_t>(msg.getNoteNumber());
            event.velocity = static_cast<uint8_t>(msg.getVelocity());
            event.hitPpqPosition = hitPpq;
            event.deltaMs = deltaMs;
            event.state = state;

            ringBuffer.push(event);
        }
    }

    // Advance internal clock position
    internalPpqPosition += numSamples * (currentBpm / 60.0) / srToUse;
}

juce::AudioProcessorEditor* MidiGridAnalyzerAudioProcessor::createEditor()
{
    return new MidiGridAnalyzerAudioProcessorEditor (*this);
}

void MidiGridAnalyzerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void MidiGridAnalyzerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

// JUCE Plugin Entry Point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MidiGridAnalyzerAudioProcessor();
}
