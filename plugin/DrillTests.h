#pragma once
#include "DrillEngine.h"

class DrillTests : public juce::UnitTest {
public:
  DrillTests () : UnitTest ("Grouping drill", "P1") {}
  void runTest () override {
    testProgression ();
    testScoring ();
    testControls ();
    testFreeEntry ();
    testDetection ();
    testReacquisition ();
  }

private:
  static DrillEngine::Config config () {
    DrillEngine::Config c;
    c.pattern[0] = 'R';
    c.pattern[1] = 'L';
    c.pattern[2] = 'K';
    c.length = 3;
    return c;
  }
  static DrillEngine started () {
    DrillEngine e;
    e.command (DrillEngine::Action::Start, config (), 0);
    e.advance (8, 8);
    return e;
  }
  static void play (DrillEngine &e, int first, int count, int errorEvery = 0, bool duplicate = false) {
    for (int i = first; i < first + count; ++i) {
      const double at = 8 + i * constants::musical::ppq_1_16;
      if (errorEvery == 0 || i % errorEvery != errorEvery - 1) {
        const int stroke = e.view.state == DrillEngine::State::Waiting ? 0 : (e.view.activeSlot + 1) % 3;
        e.hit (at, stroke == 2 ? DrumMap::Kick : DrumMap::SnareHead);
        if (duplicate) {
          e.hit (at, DrumMap::SnareHead);
        }
      }
      e.advance (at + 0.13, at + 0.13);
    }
  }
  void testProgression () {
    beginTest ("Two complete passes, announced pattern-boundary increase, settling repeat");
    auto e = started ();
    expectEquals (e.view.expected, 42);
    play (e, 0, 42);
    expectEquals (e.view.passes, 1);
    expectEquals (e.view.bpm, 60.0);
    play (e, 42, 42);
    expectEquals (e.view.best, 60.0);
    expectEquals (e.view.nextBpm, 63.0);
    const double boundary = e.nextBoundary ();
    expectWithinAbsoluteError (std::fmod (boundary - 8, 0.75), 0.0, 1e-9);
    for (double at = 29; at < boundary - 0.001; at += constants::musical::ppq_1_16) {
      e.hit (at, (e.view.activeSlot + 1) % 3 == 2 ? DrumMap::Kick : DrumMap::SnareHead);
      e.advance (at + 0.13, at + 0.13);
    }
    e.advance (boundary - 0.001, boundary - 0.001);
    expectEquals (e.view.bpm, 60.0);
    e.advance (boundary, boundary);
    expectEquals (e.view.bpm, 63.0);
    e.hit (boundary, DrumMap::Kick);
    e.advance (boundary + 0.7, boundary + 0.7);
    expectEquals (e.view.progress, 0.0);

    beginTest ("Triplet timing is independent of grouping length");
    auto c = config ();
    c.interval = constants::musical::ppq_1_8T;
    c.length = 2;
    e.command (DrillEngine::Action::Start, c, 0);
    for (int i = 0; i < 32; ++i) {
      const double at = 8 + i * c.interval;
      e.hit (at, DrumMap::HighTom);
      e.advance (at + c.interval * 0.51, at + c.interval * 0.51);
    }
    expectEquals (e.view.accuracy, 1.0);
  }
  void testScoring () {
    beginTest ("Missing notes cannot produce a perfect score and trigger step back");
    auto e = started ();
    play (e, 0, 42, 2);
    expectEquals (e.view.missing, 21);
    expectEquals (e.view.accuracy, 0.5);
    play (e, 42, 42, 2);
    expect (e.view.limitReached && !e.view.automatic);
    expectEquals (e.view.nextBpm, 48.0);
    expectEquals (e.view.best, 0.0);

    beginTest ("Duplicate hits are extras, not additional correct strokes");
    e = started ();
    play (e, 0, 42, 0, true);
    expectEquals (e.view.extras, 42);
    expectEquals (e.view.passes, 0);

    beginTest ("Wrong kick placement and late hits are separate failures; pedal is ignored");
    e = started ();
    for (int i = 0; i < 42; ++i) {
      const double at = 8 + i * 0.25;
      e.hit (at, DrumMap::PedalHiHat);
      e.hit (at + 0.03, DrumMap::SnareHead);
      e.advance (at + 0.13, at + 0.13);
    }
    expectEquals (e.view.wrong, 14);
    expectEquals (e.view.late, 28);
    expectEquals (e.view.extras, 0);

    beginTest ("Early first stroke is accepted; a missed time slot does not consume a sticking stroke");
    e = started ();
    e.hit (7.99, DrumMap::HighTom);
    e.advance (8.13, 8.13);
    play (e, 2, 40);
    expectEquals (e.view.missing, 1);
    expectEquals (e.view.wrong, 0);
    expectEquals (e.view.correct, 41);

    beginTest ("Six borderline blocks conclude the stage");
    e = started ();
    play (e, 0, 42 * 6, 10);
    expect (e.view.limitReached);
    expectEquals (e.view.blocks, 6);
  }
  void testControls () {
    beginTest ("Silence rearms listening while the click keeps running");
    auto e = started ();
    e.hit (8, DrumMap::SnareHead);
    e.advance (16, 16);
    expect (e.view.state == DrillEngine::State::Waiting && e.view.noHits && e.running ());
    e.command (DrillEngine::Action::Resume, config (), 16);
    expect (e.view.state == DrillEngine::State::Waiting && !e.view.noHits);

    beginTest ("Disconnect cancels pending acceleration without restarting the clock");
    e = started ();
    play (e, 0, 84);
    e.command (DrillEngine::Action::Disconnect, config (), 29);
    expect (!e.view.automatic);
    expectEquals (e.view.nextBpm, 0.0);
    expectEquals (e.view.bpm, 60.0);
    expect (e.view.state == DrillEngine::State::Playing);

    beginTest ("Manual limit returns to confirmed tempo, finish preserves result");
    e.command (DrillEngine::Action::TooFast, config (), 29);
    expectEquals (e.view.bpm, 60.0);
    expect (e.view.state == DrillEngine::State::Waiting);
    e.command (DrillEngine::Action::Finish, config (), 29);
    expect (!e.active ());
    expectEquals (e.view.best, 60.0);
  }
  void testReacquisition () {
    beginTest ("All cyclic entry phases of mixed and hand-only groupings are recognized");
    for (const auto *pattern : {"RLRLKK", "KKRL", "RLRL"}) {
      auto c = config ();
      const auto text = juce::String (pattern);
      c.length = text.length ();
      text.copyToUTF8 (c.pattern.data (), c.pattern.size ());
      for (int phase = 0; phase < c.length; ++phase) {
        DrillEngine e;
        e.command (DrillEngine::Action::Start, c, 0);
        for (int i = 0; i < c.length * 3; ++i) {
          const auto stroke = c.pattern[static_cast<size_t> ((i + phase) % c.length)];
          const double at = 8.25 + i * c.interval;
          e.hit (at, stroke == 'K' ? DrumMap::Kick : DrumMap::SnareHead);
          e.advance (at + c.interval * 0.51, at + c.interval * 0.51);
        }
        expect (e.view.sequenceDetected, text + " phase " + juce::String (phase));
      }
    }
    beginTest ("Missing a stroke during an announced increase cancels the increase");
    auto e = started ();
    play (e, 0, 84);
    expect (e.view.nextBpm > 0);
    e.advance (29.13, 29.13);
    expect (!e.view.sequenceDetected);
    expectEquals (e.view.nextBpm, 0.0);
    expectEquals (e.view.bpm, 60.0);
  }
  void testDetection () {
    beginTest ("A stray hand on beat one does not lock RLK to the downbeat");
    auto e = started ();
    e.hit (8, DrumMap::SnareHead);
    expect (!e.view.sequenceDetected);
    for (int i = 0; i < 6; ++i) {
      const double at = 8.25 + i * constants::musical::ppq_1_16;
      e.hit (at, i % 3 == 2 ? DrumMap::Kick : DrumMap::SnareHead);
      e.advance (at + 0.13, at + 0.13);
    }
    expect (e.view.sequenceDetected && e.view.sequenceSeen);
    expectEquals (e.view.activeSlot, 2);

    beginTest ("A wrong class loses detection; a full cyclic repetition recovers without restart");
    e.hit (9.75, DrumMap::Kick);
    expect (!e.view.sequenceDetected && e.view.sequenceSeen);
    for (int i = 0; i < 6; ++i) {
      const double at = 10 + i * constants::musical::ppq_1_16;
      e.hit (at, i % 3 == 0 ? DrumMap::Kick : DrumMap::HighTom);
      e.advance (at + 0.13, at + 0.13);
    }
    expect (e.view.sequenceDetected);
    expectEquals (e.view.activeSlot, 1);
    e.advance (11.65, 11.65);
    expect (!e.view.sequenceDetected);

    beginTest ("Triplet sequence detection uses the triplet grid across barlines");
    auto c = config ();
    c.interval = constants::musical::ppq_1_8T;
    e.command (DrillEngine::Action::Start, c, 0);
    for (int i = 0; i < 6; ++i) {
      const double at = (11 + i) * c.interval;
      e.hit (at, i % 3 == 2 ? DrumMap::Kick : DrumMap::SnareHead);
      e.advance (at + c.interval * 0.51, at + c.interval * 0.51);
    }
    expect (e.view.sequenceDetected);

    beginTest ("Tolerance updates preserve tempo and sequence but clear mixed confirmation");
    e = started ();
    play (e, 0, 84);
    expect (e.view.best > 0 && e.view.nextBpm > 0);
    c = config ();
    c.tolerance = 35;
    e.command (DrillEngine::Action::Tolerance, c, 29);
    expectEquals (e.view.bpm, 60.0);
    expectEquals (e.view.toleranceMs, 35.0);
    expectEquals (e.view.best, 0.0);
    expectEquals (e.view.nextBpm, 0.0);
    expectEquals (e.view.passes, 0);
    expect (e.view.sequenceDetected);
  }
  void testFreeEntry () {
    beginTest ("Wait indefinitely, ignore wrong starting class, and start on any subdivision");
    auto e = started ();
    e.advance (123, 123);
    expect (e.view.state == DrillEngine::State::Waiting && e.running ());
    expectEquals (e.view.missing, 0);
    e.hit (123, DrumMap::Kick);
    expect (e.view.state == DrillEngine::State::Waiting);
    for (int i = 0; i < 42; ++i) {
      const double at = 123.25 + i * constants::musical::ppq_1_16;
      e.hit (at, i % 3 == 2 ? DrumMap::Kick : DrumMap::SnareHead);
      e.advance (at + 0.13, at + 0.13);
    }
    expectEquals (e.view.accuracy, 1.0);
    expectEquals (e.view.passes, 1);

    beginTest ("A late first hit does not move the metronome grid to hide the error");
    e = started ();
    e.hit (8.08, DrumMap::SnareHead);
    e.advance (8.13, 8.13);
    play (e, 1, 41);
    expectEquals (e.view.late, 1);
    expectEquals (e.view.wrong, 0);

    beginTest ("After a gap the next received stroke is still L, not K");
    e = started ();
    e.hit (8, DrumMap::SnareHead);
    e.advance (8.38, 8.38);
    e.hit (8.5, DrumMap::HighTom);
    e.advance (8.63, 8.63);
    play (e, 3, 39);
    expectEquals (e.view.missing, 1);
    expectEquals (e.view.wrong, 0);
    expectEquals (e.view.correct, 41);

    beginTest ("Kick-first patterns arm only on a kick, and restart without a forced count-in");
    auto c = config ();
    c.pattern[0] = 'K';
    c.pattern[1] = 'R';
    c.pattern[2] = 'L';
    e.command (DrillEngine::Action::Start, c, 0);
    e.hit (30, DrumMap::SnareHead);
    expect (e.view.state == DrillEngine::State::Waiting);
    e.hit (30.25, DrumMap::Kick);
    expect (e.view.state == DrillEngine::State::Playing);
    e.advance (40, 40);
    expect (e.view.state == DrillEngine::State::Waiting);
    e.hit (50.75, DrumMap::Kick);
    expect (e.view.state == DrillEngine::State::Playing);
    expectEquals (e.view.activeSlot, 0);
  }
};
static DrillTests drillTests;

#include "PluginProcessor.h"
class DrillProcessorTests : public juce::UnitTest {
public:
  DrillProcessorTests () : UnitTest ("Drill processor integration", "P1") {}
  void runTest () override {
    testSubdivisions ();
    testIntegration ();
    testTolerance ();
  }

private:
  void testTolerance () {
    beginTest ("Drill inherits main tolerance, overrides live hit colors, and leaves main setting intact");
    MidiGridAnalyzerAudioProcessor p;
    p.prepareToPlay (44100, 512);
    p.isStandaloneMode = true;
    p.setDeviceLatencySamples (0, 0);
    auto *mainTolerance = p.apvts.getParameter ("tolerance_ms");
    mainTolerance->setValueNotifyingHost (mainTolerance->convertTo0to1 (10));
    expect (p.drillCommand (juce::JSON::parse (R"({"action":"start","pattern":"RLK","spacing":2,"bpm":60})")));
    juce::AudioBuffer<float> audio (2, 512);
    juce::MidiBuffer midi;
    p.processBlock (audio, midi);
    expectEquals (p.getDrillSnapshot ().config.tolerance, 10.0f);
    expect (!p.drillCommand (juce::JSON::parse (R"({"action":"tolerance","tolerance":100})")));
    expect (p.drillCommand (juce::JSON::parse (R"({"action":"tolerance","tolerance":35})")));
    p.internalPpqPosition = 1.03;
    midi.addEvent (juce::MidiMessage::noteOn (1, DrumMap::SnareHead, static_cast<juce::uint8> (100)), 0);
    p.processBlock (audio, midi);
    expectEquals (p.getDrillSnapshot ().toleranceMs, 35.0);
    HitEvent hit;
    expect (p.ringBuffer.pop (hit));
    expect (hit.state == TimingState::OnGrid);
    expectEquals (p.apvts.getRawParameterValue ("tolerance_ms")->load (), 10.0f);
    const auto wire = juce::JSON::parse (p.getDrillStateJson ());
    expectEquals (static_cast<double> (wire["toleranceOverride"]), 35.0);
    expect (wire.hasProperty ("sequenceDetected") && wire.hasProperty ("sequenceSeen"));
    expect (p.drillCommand (juce::JSON::parse (R"({"action":"finish"})")));
    midi.clear ();
    p.processBlock (audio, midi);
    expectEquals (p.apvts.getRawParameterValue ("tolerance_ms")->load (), 10.0f);
    expect (!p.drillCommand (juce::JSON::parse (R"({"action":"tolerance","tolerance":25})")));
    expect (p.drillCommand (juce::JSON::parse (R"({"action":"start","pattern":"RLK","spacing":2,"bpm":60})")));
    p.processBlock (audio, midi);
    expectEquals (p.getDrillSnapshot ().config.tolerance, 10.0f);
  }
  void testIntegration () {
    beginTest ("Validated commands, sample clock, MIDI pass-through and editor-independent scoring");
    MidiGridAnalyzerAudioProcessor p;
    p.prepareToPlay (44100, 512);
    p.isStandaloneMode = false;
    expect (!p.drillCommand (juce::JSON::parse (R"({"action":"start","pattern":"RLK","spacing":2,"bpm":60})")));
    p.isStandaloneMode = true;
    p.setDeviceLatencySamples (0, 0);
    expect (!p.drillCommand (juce::JSON::parse (R"({"action":"start","pattern":"RXK","spacing":2,"bpm":60})")));
    expect (!p.drillCommand (juce::JSON::parse (R"({"action":"start","pattern":"RLK","spacing":2,"bpm":0})")));
    expect (p.drillCommand (juce::JSON::parse (R"({"action":"start","pattern":" r l k ","spacing":2,"bpm":60})")));
    juce::AudioBuffer<float> audio (2, 512);
    int64_t tick = 32;
    bool increased = false;
    for (int block = 0; block < 3500; ++block) {
      const double start = p.internalPpqPosition;
      const double bpm = p.drill.active () ? p.drill.view.bpm : 60;
      const double end = start + 512 * bpm / (60 * 44100);
      juce::MidiBuffer midi;
      while (tick * 0.25 < end) {
        const auto at = tick * 0.25;
        if (at >= start) {
          const int sample = std::clamp (static_cast<int> (std::round ((at - start) * 60 * 44100 / bpm)), 0, 511);
          midi.addEvent (juce::MidiMessage::noteOn (1, (tick - 32) % 3 == 2 ? DrumMap::Kick : DrumMap::SnareHead,
                                                    static_cast<juce::uint8> (100)),
                         sample);
        }
        ++tick;
      }
      const int notes = midi.getNumEvents ();
      p.processBlock (audio, midi);
      expectEquals (midi.getNumEvents (), notes);
      if (p.getDrillSnapshot ().bpm == 63) {
        increased = true;
        break;
      }
    }
    expect (increased);
    expectEquals (p.getDrillSnapshot ().best, 60.0);
    expect (p.drillCommand (juce::JSON::parse (R"({"action":"finish"})")));
    juce::MidiBuffer empty;
    p.processBlock (audio, empty);
    expect (p.getDrillSnapshot ().state == DrillEngine::State::Finished);
    expectEquals (p.getAPVTS ().getRawParameterValue ("click_enabled")->load (), 0.0f);
  }
  void testSubdivisions () {
    beginTest ("Selected drill subdivisions produce audible clicks and matching hit/grid timing");
    const double intervals[]{constants::musical::ppq_1_8, constants::musical::ppq_1_8T, constants::musical::ppq_1_16};
    const int clickIndices[]{2, 4, 3};
    const int gridIndices[]{0, 1, 2};
    for (int spacing = 0; spacing < 3; ++spacing) {
      MidiGridAnalyzerAudioProcessor p;
      p.prepareToPlay (44100, 512);
      p.isStandaloneMode = true;
      p.setDeviceLatencySamples (0, 0);
      auto command = juce::JSON::parse (R"({"action":"start","pattern":"RLK","spacing":2,"bpm":60})");
      command.getDynamicObject ()->setProperty ("spacing", spacing);
      expect (p.drillCommand (command));
      expectEquals ((int)p.apvts.getRawParameterValue ("click_subdivision")->load (), clickIndices[spacing]);
      expectEquals ((int)p.apvts.getRawParameterValue ("subdivision")->load (), gridIndices[spacing]);
      p.internalPpqPosition = intervals[spacing];
      juce::AudioBuffer<float> audio (2, 512);
      juce::MidiBuffer midi;
      midi.addEvent (juce::MidiMessage::noteOn (1, DrumMap::SnareHead, static_cast<juce::uint8> (100)), 0);
      p.processBlock (audio, midi);
      expect (audio.getMagnitude (0, 512) > 0.001f, "Subdivision click must be audible between quarter notes");
      HitEvent hit;
      expect (p.ringBuffer.pop (hit));
      expectWithinAbsoluteError (hit.deltaMs, 0.0, 0.001);
      expectWithinAbsoluteError (p.getDrillSnapshot ().config.interval, intervals[spacing], 1e-9);
    }
  }
};
static DrillProcessorTests drillProcessorTests;
