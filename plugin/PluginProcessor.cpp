#include "PluginProcessor.h"

#include "DrumMap.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

MidiGridAnalyzerAudioProcessor::MidiGridAnalyzerAudioProcessor ()
    : AudioProcessor (BusesProperties ().withOutput ("Output", juce::AudioChannelSet::stereo (), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout ())
{
#if JUCE_STANDALONE_APPLICATION
    isStandaloneMode = true;
#else
    isStandaloneMode = juce::JUCEApplicationBase::isStandaloneApp ();
#endif

    // Check command line arguments for --test or --demo flags
    auto commandLineArgs = juce::JUCEApplicationBase::getCommandLineParameterArray ();
    for (const auto &arg : commandLineArgs)
    {
        if (arg.containsIgnoreCase ("test") || arg.containsIgnoreCase ("demo"))
        {
            if (auto *param = apvts.getParameter ("test_mode"))
                param->setValueNotifyingHost (1.0f);
            break;
        }
    }

    // Start remote control server for companion app (standalone only)
    if (isStandaloneMode)
    {
        remoteServer = std::make_unique<RemoteControlServer> (apvts);
        remoteServer->start ();
    }
}

MidiGridAnalyzerAudioProcessor::~MidiGridAnalyzerAudioProcessor ()
{
    if (remoteServer)
        remoteServer->stop ();
}

juce::AudioProcessorValueTreeState::ParameterLayout MidiGridAnalyzerAudioProcessor::createParameterLayout ()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID{"bars_window", 1}, "History Length", juce::StringArray{"1 Bar", "2 Bars", "4 Bars", "8 Bars"},
        2 // Default 4 Bars
        ));

    params.push_back (
        std::make_unique<juce::AudioParameterChoice> (juce::ParameterID{"subdivision", 1}, "Grid Subdivision",
                                                      juce::StringArray{"1/8", "1/8T", "1/16", "1/16T", "1/32"},
                                                      2 // Default 1/16
                                                      ));

    params.push_back (
        std::make_unique<juce::AudioParameterFloat> (juce::ParameterID{"tolerance_ms", 1}, "Timing Tolerance",
                                                     juce::NormalisableRange<float> (0.0f, 100.0f, 0.5f), 20.0f));

    params.push_back (
        std::make_unique<juce::AudioParameterFloat> (juce::ParameterID{"latency_offset_ms", 1}, "System Latency Offset",
                                                     juce::NormalisableRange<float> (-500.0f, 500.0f, 1.0f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterInt> (juce::ParameterID{"min_velocity", 1},
                                                                 "Velocity Noise Floor", 1, 127, 5));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID{"internal_bpm", 1}, "Internal BPM",
                                                                   juce::NormalisableRange<float> (40.0f, 300.0f, 0.1f),
                                                                   120.0f));

    params.push_back (std::make_unique<juce::AudioParameterInt> (juce::ParameterID{"time_sig_num", 1},
                                                                 "Time Sig Numerator", 2, 12, 4));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID{"click_subdivision", 1}, "Click Subdivision",
        juce::StringArray{"Off", "1/4 Notes", "1/8 Notes", "1/16 Notes", "Triplets"},
        1 // Default 1/4 Notes
        ));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID{"click_sample_preset", 1}, "Click Sound Preset",
        juce::StringArray{"Wood Clave", "Drum Stick Click", "Digital Beep", "Cowbell"},
        0 // Default Wood Clave
        ));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID{"click_volume", 1}, "Click Volume",
                                                                   juce::NormalisableRange<float> (0.0f, 2.0f, 0.01f),
                                                                   0.8f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID{"click_pan", 1}, "Click Panning", juce::NormalisableRange<float> (-1.0f, 1.0f, 0.05f), 0.0f));

    params.push_back (
        std::make_unique<juce::AudioParameterBool> (juce::ParameterID{"click_enabled", 1}, "Metronome On/Off", true));

    params.push_back (
        std::make_unique<juce::AudioParameterBool> (juce::ParameterID{"is_paused", 1}, "Pause/Freeze Grid", false));

    params.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID{"show_ms_labels", 1},
                                                                  "Display MS Offsets", true));

    params.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID{"show_velocity_labels", 1},
                                                                  "Display Velocity", false));

    params.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID{"show_note_numbers", 1},
                                                                  "Display Note Numbers", false));

    params.push_back (
        std::make_unique<juce::AudioParameterBool> (juce::ParameterID{"test_mode", 1}, "Rock Beat Demo Mode", false));

    params.push_back (
        std::make_unique<juce::AudioParameterChoice> (juce::ParameterID{"note_filter", 1}, "Display Mode",
                                                      juce::StringArray{"All Notes", "Roland/GM Drum Map", "Custom"},
                                                      1 // Default Roland/GM Drum Map
                                                      ));

    return {params.begin (), params.end ()};
}

double MidiGridAnalyzerAudioProcessor::getSubdivisionPpq (int index) noexcept
{
    switch (index)
    {
    case 0:
        return 0.5; // 1/8
    case 1:
        return 0.5 * (2.0 / 3.0); // 1/8T
    case 2:
        return 0.25; // 1/16
    case 3:
        return 0.25 * (2.0 / 3.0); // 1/16T
    case 4:
        return 0.125; // 1/32
    default:
        return 0.25;
    }
}

void MidiGridAnalyzerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    internalPpqPosition = 0.0;
    lastTestBeatTick = -1.0;
    ringBuffer.reset ();
    clickGenerator.prepareToPlay (sampleRate);
}

void MidiGridAnalyzerAudioProcessor::releaseResources () {}

bool MidiGridAnalyzerAudioProcessor::isBusesLayoutSupported (const BusesLayout &layouts) const
{
    if (layouts.getMainOutputChannelSet () != juce::AudioChannelSet::mono () &&
        layouts.getMainOutputChannelSet () != juce::AudioChannelSet::stereo ())
        return false;

    return true;
}

void MidiGridAnalyzerAudioProcessor::processBlock (juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
    buffer.clear (); // Clear audio buffer before rendering click audio

    const double sampleRate = getSampleRate ();
    const int numSamples = buffer.getNumSamples ();

    // Read parameter values safely
    const int subChoice = static_cast<int> (apvts.getRawParameterValue ("subdivision")->load ());
    const float toleranceMs = apvts.getRawParameterValue ("tolerance_ms")->load ();
    const float userLatencyMs = apvts.getRawParameterValue ("latency_offset_ms")->load ();
    const int minVelocity = static_cast<int> (apvts.getRawParameterValue ("min_velocity")->load ());
    const float internalBpmVal = apvts.getRawParameterValue ("internal_bpm")->load ();
    const int timeSigNumVal = static_cast<int> (apvts.getRawParameterValue ("time_sig_num")->load ());
    const int clickSubChoice = static_cast<int> (apvts.getRawParameterValue ("click_subdivision")->load ());
    const int clickPresetVal = static_cast<int> (apvts.getRawParameterValue ("click_sample_preset")->load ());
    const float clickVolVal = apvts.getRawParameterValue ("click_volume")->load ();
    const float clickPanVal = apvts.getRawParameterValue ("click_pan")->load ();
    const bool clickEnabledVal = (apvts.getRawParameterValue ("click_enabled")->load () > 0.5f);
    const bool isPausedVal = (apvts.getRawParameterValue ("is_paused")->load () > 0.5f);
    const bool testModeVal = (apvts.getRawParameterValue ("test_mode")->load () > 0.5f);

    const double gridInterval = getSubdivisionPpq (subChoice);

    // Sync with host playhead or fallback to internal clock
    bool hostPpqValid = false;
    double hostBpm = internalBpmVal;
    double hostPpq = internalPpqPosition;
    int hostTimeSigNum = timeSigNumVal;
    bool hostPlaying = false;

    if (auto playHead = getPlayHead ())
    {
        if (auto pos = playHead->getPosition ())
        {
            if (auto bpmOpt = pos->getBpm ())
                if (*bpmOpt > 0.0)
                    hostBpm = *bpmOpt;

            if (auto ppqOpt = pos->getPpqPosition ())
            {
                hostPpq = *ppqOpt;
                hostPpqValid = true;
            }

            if (auto tsOpt = pos->getTimeSignature ())
            {
                if (tsOpt->numerator > 0)
                    hostTimeSigNum = tsOpt->numerator;
            }

            hostPlaying = pos->getIsPlaying ();
        }
    }

    currentBpm = (hostBpm > 0.0) ? hostBpm : internalBpmVal;
    currentPpqPosition = hostPpqValid ? hostPpq : internalPpqPosition;
    currentTimeSigNum = (hostTimeSigNum > 0) ? hostTimeSigNum : timeSigNumVal;
    hostIsPlaying = (hostPlaying || !hostPpqValid) && !isPausedVal;

    const double srToUse = (sampleRate > 0.0) ? sampleRate : 44100.0;
    const double blockStartPpq = currentPpqPosition;
    const double blockEndPpq = blockStartPpq + numSamples * (currentBpm / 60.0) / srToUse;

    // Render Audio Click ONLY in Standalone mode when NOT paused
    if (isStandaloneMode && !isPausedVal)
    {
        clickGenerator.renderBlock (buffer, numSamples, srToUse, blockStartPpq, currentBpm, currentTimeSigNum,
                                    clickSubChoice, clickPresetVal, clickVolVal, clickPanVal, clickEnabledVal);
    }

    const double autoLatencyMs = (static_cast<double> (getLatencySamples ()) / srToUse) * 1000.0;
    const double totalLatencyMs = autoLatencyMs + static_cast<double> (userLatencyMs);
    const double totalLatencyPpq = (totalLatencyMs / 1000.0) * (currentBpm / 60.0);

    // Process incoming physical MIDI events
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage ();

        if (msg.isNoteOn () && msg.getVelocity () >= minVelocity &&
            !DrumMap::isExcluded (static_cast<uint8_t> (msg.getNoteNumber ())))
        {
            const int sampleOffset = metadata.samplePosition;
            const double rawHitPpq = currentPpqPosition + (sampleOffset * (currentBpm / 60.0) / srToUse);

            // Shift hit PPQ by total system latency so circle node snaps onto visual grid line
            const double compensatedHitPpq = rawHitPpq - totalLatencyPpq;

            const double nearestGridPpq = std::round (compensatedHitPpq / gridInterval) * gridInterval;
            const double deltaPpq = compensatedHitPpq - nearestGridPpq;
            const double deltaMs = (deltaPpq / (currentBpm / 60.0)) * 1000.0;

            const float normalizedDev =
                std::clamp (static_cast<float> (deltaMs / static_cast<double> (toleranceMs)), -1.0f, 1.0f);

            TimingState state = TimingState::OnGrid;
            if (std::abs (deltaMs) <= static_cast<double> (toleranceMs))
            {
                state = TimingState::OnGrid;
            }
            else if (deltaMs < -static_cast<double> (toleranceMs))
            {
                state = TimingState::Rush;
            }
            else
            {
                state = TimingState::Drag;
            }

            HitEvent event;
            event.noteNumber = static_cast<uint8_t> (msg.getNoteNumber ());
            event.velocity = static_cast<uint8_t> (msg.getVelocity ());
            event.rawHitPpqPosition = rawHitPpq;
            event.hitPpqPosition = compensatedHitPpq;
            event.deltaMs = deltaMs;
            event.normalizedDeviation = normalizedDev;
            event.state = state;

            ringBuffer.push (event);
        }
    }

    // Synthesize Humanized Rock Drum Beat in TEST / DEMO Mode
    if (testModeVal && !isPausedVal && currentTimeSigNum > 0)
    {
        generateTestModeBeat (blockStartPpq, blockEndPpq, totalLatencyPpq, gridInterval, toleranceMs);
    }

    // Advance internal clock position ONLY when not paused
    if (!isPausedVal)
    {
        internalPpqPosition += numSamples * (currentBpm / 60.0) / srToUse;
    }
}

void MidiGridAnalyzerAudioProcessor::generateTestModeBeat (double blockStartPpq, double blockEndPpq,
                                                           double totalLatencyPpq, double gridInterval,
                                                           float toleranceMs)
{
    const double subInterval = 0.5; // 8th notes
    const double firstTick = std::floor (blockStartPpq / subInterval) * subInterval;

    for (double tick = firstTick; tick < blockEndPpq; tick += subInterval)
    {
        if (tick >= blockStartPpq && tick > lastTestBeatTick)
        {
            lastTestBeatTick = tick;

            const double barPpq = std::fmod (tick, static_cast<double> (currentTimeSigNum));
            const int beatInBar = static_cast<int> (std::floor (barPpq));
            const double subFraction = std::fmod (tick, 1.0);
            const bool is8thAnd = (std::abs (subFraction - 0.5) < 0.001);

            auto pushTestHit =
                [this, tick, totalLatencyPpq, gridInterval, toleranceMs] (uint8_t note, uint8_t vel, double devMs)
            {
                const double devPpq = (devMs / 1000.0) * (currentBpm / 60.0);
                const double targetCompPpq = tick + devPpq;
                const double rawPpq = targetCompPpq + totalLatencyPpq;

                const double nearestGrid = std::round (targetCompPpq / gridInterval) * gridInterval;
                const double deltaPpq = targetCompPpq - nearestGrid;
                const double deltaMs = (deltaPpq / (currentBpm / 60.0)) * 1000.0;
                const float normDev =
                    std::clamp (static_cast<float> (deltaMs / static_cast<double> (toleranceMs)), -1.0f, 1.0f);

                HitEvent e;
                e.noteNumber = note;
                e.velocity = vel;
                e.rawHitPpqPosition = rawPpq;
                e.hitPpqPosition = targetCompPpq;
                e.deltaMs = deltaMs;
                e.normalizedDeviation = normDev;
                e.state = (std::abs (deltaMs) <= toleranceMs) ? TimingState::OnGrid
                                                              : ((deltaMs < 0) ? TimingState::Rush : TimingState::Drag);
                ringBuffer.push (e);
            };

            auto generateTestDevMs = [this, toleranceMs] () -> double
            {
                const float roll = random.nextFloat ();
                if (roll < 0.80f)
                {
                    // 80% On-Grid (Inside tolerance range -> Green checkmark)
                    const double factor = static_cast<double> (random.nextFloat () * 1.4f - 0.7f); // [-0.7, +0.7]
                    return factor * static_cast<double> (toleranceMs);
                }
                else if (roll < 0.90f)
                {
                    // 10% Rush / Early -> Yellow -> Orange -> Electric Red
                    const double factor = static_cast<double> (1.25f + random.nextFloat () * 0.95f); // [-2.2, -1.25]
                    return -factor * static_cast<double> (toleranceMs);
                }
                else
                {
                    // 10% Drag / Late -> Cyan -> Deep Blue -> Vivid Purple
                    const double factor = static_cast<double> (1.25f + random.nextFloat () * 0.95f); // [+1.25, +2.2]
                    return factor * static_cast<double> (toleranceMs);
                }
            };

            // 1. Hi-Hat on all 8th notes (humanized with 20% out-of-tolerance error distribution)
            pushTestHit (DrumMap::ClosedHiHat,
                         static_cast<uint8_t> (DrumMap::TestModeVelocity::BaseHiHat +
                                               random.nextInt (DrumMap::TestModeVelocity::HiHatRange)),
                         generateTestDevMs ());

            // 2. Kick on Beats 1 & 3
            if (!is8thAnd && (beatInBar == 0 || beatInBar == 2))
            {
                pushTestHit (DrumMap::Kick,
                             static_cast<uint8_t> (DrumMap::TestModeVelocity::BaseKick +
                                                   random.nextInt (DrumMap::TestModeVelocity::KickRange)),
                             generateTestDevMs ());
            }

            // 3. Snare on Beats 2 & 4
            if (!is8thAnd && (beatInBar == 1 || beatInBar == 3))
            {
                pushTestHit (DrumMap::SnareHead,
                             static_cast<uint8_t> (DrumMap::TestModeVelocity::BaseSnare +
                                                   random.nextInt (DrumMap::TestModeVelocity::SnareRange)),
                             generateTestDevMs ());
            }

            // 4. Crash Cymbal on Bar 1, Beat 1
            if (!is8thAnd && beatInBar == 0 &&
                std::abs (std::fmod (tick, static_cast<double> (currentTimeSigNum) * 4.0)) < 0.001)
            {
                pushTestHit (DrumMap::Crash1, DrumMap::TestModeVelocity::Crash, generateTestDevMs ());
            }
        }
    }
}

juce::AudioProcessorEditor *MidiGridAnalyzerAudioProcessor::createEditor ()
{
    return new MidiGridAnalyzerAudioProcessorEditor (*this);
}

void MidiGridAnalyzerAudioProcessor::getStateInformation (juce::MemoryBlock &destData)
{
    auto state = apvts.copyState ();
    std::unique_ptr<juce::XmlElement> xml (state.createXml ());
    copyXmlToBinary (*xml, destData);
}

void MidiGridAnalyzerAudioProcessor::setStateInformation (const void *data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get () != nullptr)
        if (xmlState->hasTagName (apvts.state.getType ()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

// JUCE Plugin Entry Point
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter ()
{
    return new MidiGridAnalyzerAudioProcessor ();
}
