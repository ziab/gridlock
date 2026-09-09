#pragma once
#include "DrillEngine.h"

class DrillTests : public juce::UnitTest {
public:
  DrillTests () : UnitTest ("Grouping drill", "P1") {}
  void runTest () override {
    testProgression ();
    testScoring ();
    testControls ();
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
      if (errorEvery == 0 || i % errorEvery != 0) {
        e.hit (at, i % 3 == 2 ? DrumMap::Kick : DrumMap::SnareHead);
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

    beginTest ("Early first stroke is accepted; one omitted slot never shifts the sequence");
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
    beginTest ("Silence pauses; resuming counts in and clears assessment");
    auto e = started ();
    e.advance (16, 16);
    expect (e.view.state == DrillEngine::State::Paused && e.view.noHits);
    e.command (DrillEngine::Action::Resume, config (), 16);
    expect (e.view.state == DrillEngine::State::CountIn && !e.view.noHits);

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
    expect (e.view.state == DrillEngine::State::CountIn);
    e.command (DrillEngine::Action::Finish, config (), 29);
    expect (!e.active ());
    expectEquals (e.view.best, 60.0);
  }
};
static DrillTests drillTests;

#include "PluginProcessor.h"
class DrillProcessorTests : public juce::UnitTest {
public:
  DrillProcessorTests () : UnitTest ("Drill processor integration", "P1") {}
  void runTest () override {
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
};
static DrillProcessorTests drillProcessorTests;
