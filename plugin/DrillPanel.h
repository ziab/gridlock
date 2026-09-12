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
    tolerance.setRange (constants::params::toleranceMin, constants::params::toleranceMax,
                        constants::params::toleranceStep);
    tolerance.setValue (processor.getAPVTS ().getRawParameterValue ("tolerance_ms")->load ());
    tolerance.setTextValueSuffix (" ms tolerance");
    tolerance.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 150, 24);
    pattern.onTextChange = [this] { suggestTempo (); };
    kick.onValueChange = [this] { suggestTempo (); };
    tolerance.onValueChange = [this] {
      if (processor.isDrillActive ()) {
        juce::DynamicObject::Ptr message = new juce::DynamicObject ();
        message->setProperty ("action", "tolerance");
        message->setProperty ("tolerance", tolerance.getValue ());
        processor.drillCommand (juce::var (message.get ()));
      } else {
        suggestTempo ();
      }
    };
    tolerance.setTooltip ("Only this drill. Changing tolerance resets tempo confirmation.");
    addAndMakeVisible (pass);
    pass.setRange (constants::drill::passThresholdMin * 100, constants::drill::passThresholdMax * 100,
                   constants::drill::passThresholdStep * 100);
    pass.setValue (constants::drill::passThresholdDefault * 100);
    pass.setTextValueSuffix ("% to pass");
    pass.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 150, 24);
    pass.setTooltip ("Only this drill. Changing the pass bar resets tempo confirmation.");
    pass.onValueChange = [this] {
      if (processor.isDrillActive ()) {
        juce::DynamicObject::Ptr message = new juce::DynamicObject ();
        message->setProperty ("action", "pass_threshold");
        message->setProperty ("passThreshold", pass.getValue () / 100);
        processor.drillCommand (juce::var (message.get ()));
      } else {
        suggestTempo ();
      }
    };
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
    setSize (600, 490);
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
      tolerance.setBounds (area.removeFromTop (36));
      pass.setBounds (area.removeFromTop (36));
      status.setBounds (area.removeFromTop (110));
      layoutButtons (area);
      return;
    }
    auto row = area.removeFromTop (36);
    pattern.setBounds (row.removeFromLeft (area.getWidth () / 2).reduced (3));
    spacing.setBounds (row.reduced (3));
    bpm.setBounds (area.removeFromTop (42));
    kick.setBounds (area.removeFromTop (36));
    tolerance.setBounds (area.removeFromTop (36));
    pass.setBounds (area.removeFromTop (36));
    help.setBounds (area.removeFromTop (52));
    status.setBounds (area.removeFromTop (110));
    layoutButtons (area);
  }

private:
  MidiGridAnalyzerAudioProcessor &processor;
  juce::TextEditor pattern;
  juce::ComboBox spacing;
  juce::Slider bpm, kick, tolerance, pass;
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
                                                     static_cast<int> (kick.getValue ()), tolerance.getValue (),
                                                     pass.getValue () / 100));
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
      message->setProperty ("passThreshold", pass.getValue () / 100);
      error = processor.drillCommand (juce::var (message.get ())) ? "" : "Enter 1–64 R/L/K letters and valid settings.";
    };
  }
  void timerCallback () override {
    const auto s = processor.getDrillSnapshot ();
    const bool active = s.state != DrillEngine::State::Idle && s.state != DrillEngine::State::Finished;
    for (auto *control : std::array<juce::Component *, 5>{&pattern, &spacing, &bpm, &kick, &start}) {
      control->setEnabled (!active);
    }
    for (auto *control : {&hold, &slower, &pause, &retry, &finish}) {
      control->setEnabled (active);
    }
    if (active) {
      pattern.setText (juce::String (s.config.pattern.data ()), false);
      bpm.setValue (s.bpm, juce::dontSendNotification);
      kick.setValue (s.config.kick, juce::dontSendNotification);
      if (!tolerance.isMouseButtonDown ()) {
        tolerance.setValue (s.config.tolerance, juce::dontSendNotification);
      }
      if (!pass.isMouseButtonDown ()) {
        pass.setValue (s.config.passThreshold * 100, juce::dontSendNotification);
      }
      const int id = s.config.interval == constants::musical::ppq_1_8    ? 1
                     : s.config.interval == constants::musical::ppq_1_8T ? 2
                                                                         : 3;
      spacing.setSelectedId (id, juce::dontSendNotification);
    }
    if (liveLayout != active) {
      liveLayout = active;
      if (!active) {
        tolerance.setValue (processor.getAPVTS ().getRawParameterValue ("tolerance_ms")->load (),
                            juce::dontSendNotification);
      }
      for (auto *control : std::array<juce::Component *, 4>{&pattern, &spacing, &bpm, &kick}) {
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
      patternLine.setText (letters, juce::dontSendNotification);
      patternLine.setColour (juce::Label::textColourId,
                             Theme::col (s.sequenceDetected ? Theme::emerald : Theme::textPrimary));
      text = active ? juce::String () : letters + "   " + juce::String (s.bpm, 0) + " BPM";
      if (s.state == DrillEngine::State::Waiting) {
        text += "Play when ready — any subdivision";
      } else if (s.state == DrillEngine::State::Paused) {
        text += s.noHits ? " — No hits detected" : " — Paused";
      } else if (s.state == DrillEngine::State::Finished) {
        text += " — Finished";
      } else {
        text += s.automatic ? " — Climbing" : " — Holding";
      }
      if (active && s.state != DrillEngine::State::Paused) {
        text += s.sequenceDetected ? " | Sequence detected" : s.sequenceSeen ? " | Sequence lost" : " | Listening";
      }
      text += "\nWindow: " + juce::String (s.toleranceMs, 1) + " ms (drill only)";
      text += "\nNeed ≥" + juce::String (static_cast<int> (std::round (s.config.passThreshold * 100))) +
              "% + locked sequence, 2 in a row";
      text += "\nHighest confirmed: " + (s.best > 0 ? juce::String (s.best, 0) : "none") + " | " +
              juce::String (s.passes) + "/2 passes | Floor " + juce::String (s.floorBpm, 0);
      text += " | Block " + juce::String (s.progress * 100, 0) + "%";
      text += "\nLast block: " + juce::String (s.accuracy * 100, 0) + "% | Miss " + juce::String (s.missing) +
              " Wrong " + juce::String (s.wrong) + " Timing " + juce::String (s.late) + " Extra " +
              juce::String (s.extras);
      if (s.hasBlock) {
        if (!s.failAccuracy && !s.failExtras && !s.failSequence) {
          text += " — pass";
        } else {
          juce::StringArray reasons;
          if (s.failAccuracy) {
            reasons.add ("need ≥" + juce::String (static_cast<int> (std::round (s.config.passThreshold * 100))) + "%");
          }
          if (s.failExtras) {
            reasons.add ("too many extras");
          }
          if (s.failSequence) {
            reasons.add ("sequence not locked");
          }
          text += " — " + reasons.joinIntoString (", ");
        }
      }
      if (s.nextBpm > 0) {
        text += "\nNext repeat: " + juce::String (s.nextBpm, 0) + " BPM";
      }
    }
    status.setText (error.isNotEmpty () ? error : text, juce::dontSendNotification);
  }
};
