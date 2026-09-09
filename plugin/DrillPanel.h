#pragma once

#include "PluginProcessor.h"
#include "Theme.h"

class DrillPanel : public juce::Component, private juce::Timer {
public:
  explicit DrillPanel (MidiGridAnalyzerAudioProcessor &p) : processor (p) {
    addAndMakeVisible (pattern);
    pattern.setText ("RLK");
    pattern.setInputRestrictions (constants::drill::maxPattern);
    pattern.setTooltip ("Pattern: R and L are hand hits; K is kick. Spaces are allowed.");
    addAndMakeVisible (spacing);
    spacing.addItemList ({"Eighth notes", "Triplets", "Sixteenth notes"}, 1);
    spacing.setSelectedId (3);
    spacing.onChange = [this] { suggestTempo (); };
    addAndMakeVisible (bpm);
    bpm.setRange (constants::params::bpmMin, constants::params::bpmMax, 1);
    bpm.setValue (constants::drill::startBpm);
    bpm.setTextValueSuffix (" BPM");
    addAndMakeVisible (kick);
    kick.setRange (0, 127, 1);
    kick.setValue (DrumMap::Kick);
    kick.setTextValueSuffix (" kick note");
    addAndMakeVisible (tolerance);
    tolerance.setRange (constants::params::toleranceMin, constants::params::toleranceMax, 1);
    tolerance.setValue (constants::params::toleranceDefault);
    tolerance.setTextValueSuffix (" ms tolerance");
    pattern.onTextChange = [this] { suggestTempo (); };
    kick.onValueChange = [this] { suggestTempo (); };
    tolerance.onValueChange = [this] { suggestTempo (); };
    suggestTempo ();
    setupButton (start, "Start", "start");
    setupButton (hold, "Hold tempo", "hold");
    setupButton (slower, "Too fast", "too_fast");
    setupButton (pause, "Pause", "pause");
    setupButton (retry, "Try again", "retry");
    setupButton (finish, "Finish", "finish");
    addChildComponent (headline);
    headline.setFont (juce::Font (44));
    headline.setJustificationType (juce::Justification::centred);
    addChildComponent (patternLine);
    patternLine.setFont (juce::Font (28));
    patternLine.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (status);
    status.setJustificationType (juce::Justification::centred);
    status.setFont (juce::Font (18));
    addAndMakeVisible (help);
    help.setText ("R/L sticking is your responsibility. Timing and kick placement are checked.\nEach letter is one "
                  "subdivision; the pattern continues across barlines.",
                  juce::dontSendNotification);
    help.setJustificationType (juce::Justification::centred);
    setSize (600, 450);
    startTimerHz (constants::network::pollHz);
    timerCallback ();
  }
  void paint (juce::Graphics &g) override {
    g.fillAll (Theme::col (Theme::bgMain));
  }
  void resized () override {
    auto area = getLocalBounds ().reduced (16);
    if (liveLayout) {
      headline.setBounds (area.removeFromTop (64));
      patternLine.setBounds (area.removeFromTop (56));
      help.setBounds (area.removeFromTop (52));
      status.setBounds (area.removeFromTop (136));
      layoutButtons (area);
      return;
    }
    auto row = area.removeFromTop (36);
    pattern.setBounds (row.removeFromLeft (area.getWidth () / 2).reduced (3));
    spacing.setBounds (row.reduced (3));
    bpm.setBounds (area.removeFromTop (42));
    kick.setBounds (area.removeFromTop (36));
    tolerance.setBounds (area.removeFromTop (36));
    help.setBounds (area.removeFromTop (52));
    status.setBounds (area.removeFromTop (110));
    layoutButtons (area);
  }

private:
  MidiGridAnalyzerAudioProcessor &processor;
  juce::TextEditor pattern;
  juce::ComboBox spacing;
  juce::Slider bpm, kick, tolerance;
  juce::Label status, help, headline, patternLine;
  bool liveLayout{false};
  juce::TextButton start, hold, slower, pause, retry, finish;
  juce::String error;
  void layoutButtons (juce::Rectangle<int> area) {
    const int width = area.getWidth () / 3;
    auto row = area.removeFromTop (40);
    for (auto *button : {&start, &hold, &slower}) {
      button->setBounds (row.removeFromLeft (width).reduced (3));
    }
    row = area.removeFromTop (40);
    for (auto *button : {&pause, &retry, &finish}) {
      button->setBounds (row.removeFromLeft (width).reduced (3));
    }
  }
  void suggestTempo () {
    bpm.setValue (processor.drillHistory.suggestion (pattern.getText (), spacing.getSelectedId () - 1,
                                                     static_cast<int> (kick.getValue ()), tolerance.getValue ()));
  }
  void setupButton (juce::TextButton &button, const char *title, const char *action) {
    button.setButtonText (title);
    addAndMakeVisible (button);
    button.onClick = [this, action] {
      juce::DynamicObject::Ptr message = new juce::DynamicObject ();
      auto command = juce::String (action);
      if (command == "pause" && processor.getDrillSnapshot ().state == DrillEngine::State::Paused) {
        command = "resume";
      }
      message->setProperty ("action", command);
      message->setProperty ("pattern", pattern.getText ());
      message->setProperty ("spacing", spacing.getSelectedId () - 1);
      message->setProperty ("bpm", bpm.getValue ());
      message->setProperty ("kick", kick.getValue ());
      message->setProperty ("tolerance", tolerance.getValue ());
      error = processor.drillCommand (juce::var (message.get ())) ? "" : "Enter 1–64 R/L/K letters and valid settings.";
    };
  }
  void timerCallback () override {
    const auto s = processor.getDrillSnapshot ();
    const bool active = s.state != DrillEngine::State::Idle && s.state != DrillEngine::State::Finished;
    for (auto *control : std::array<juce::Component *, 6>{&pattern, &spacing, &bpm, &kick, &tolerance, &start}) {
      control->setEnabled (!active);
    }
    for (auto *control : {&hold, &slower, &pause, &retry, &finish}) {
      control->setEnabled (active);
    }
    if (active) {
      pattern.setText (juce::String (s.config.pattern.data ()), false);
      bpm.setValue (s.bpm, juce::dontSendNotification);
      kick.setValue (s.config.kick, juce::dontSendNotification);
      tolerance.setValue (s.config.tolerance, juce::dontSendNotification);
      const int id = s.config.interval == constants::musical::ppq_1_8    ? 1
                     : s.config.interval == constants::musical::ppq_1_8T ? 2
                                                                         : 3;
      spacing.setSelectedId (id, juce::dontSendNotification);
    }
    if (liveLayout != active) {
      liveLayout = active;
      for (auto *control : std::array<juce::Component *, 5>{&pattern, &spacing, &bpm, &kick, &tolerance}) {
        control->setVisible (!active);
      }
      headline.setVisible (active);
      patternLine.setVisible (active);
      resized ();
    }
    headline.setText (juce::String (s.bpm, 0) + " BPM", juce::dontSendNotification);
    pause.setButtonText (s.state == DrillEngine::State::Paused ? "Resume" : "Pause");
    updateStatus (s, active);
  }
  void updateStatus (const DrillEngine::Snapshot &s, bool active) {
    juce::String text;
    if (s.state == DrillEngine::State::Idle) {
      text = "Enter a grouping and press Start";
    } else {
      auto letters = juce::String (s.config.pattern.data ());
      if (active) {
        letters = letters.substring (0, s.activeSlot) + "[" + letters.substring (s.activeSlot, s.activeSlot + 1) + "]" +
                  letters.substring (s.activeSlot + 1);
      }
      patternLine.setText (letters, juce::dontSendNotification);
      text = active ? juce::String () : letters + "   " + juce::String (s.bpm, 0) + " BPM";
      if (s.state == DrillEngine::State::CountIn) {
        text += " — Count in: " + juce::String (s.beatsRemaining);
      } else if (s.state == DrillEngine::State::Paused) {
        text += s.noHits ? " — No hits detected" : " — Paused";
      } else if (s.state == DrillEngine::State::Finished) {
        text += " — Finished";
      } else {
        text += s.automatic ? " — Climbing" : " — Holding";
      }
      text += "\nHighest confirmed: " + (s.best > 0 ? juce::String (s.best, 0) : "none") + " | " +
              juce::String (s.passes) + "/2 passes";
      text += " | Block " + juce::String (s.progress * 100, 0) + "%";
      text += "\nLast block: " + juce::String (s.accuracy * 100, 0) + "% | Miss " + juce::String (s.missing) +
              " Wrong " + juce::String (s.wrong) + " Timing " + juce::String (s.late) + " Extra " +
              juce::String (s.extras);
      if (s.nextBpm > 0) {
        text += "\nNext repeat: " + juce::String (s.nextBpm, 0) + " BPM";
      }
    }
    status.setText (error.isNotEmpty () ? error : text, juce::dontSendNotification);
  }
};
