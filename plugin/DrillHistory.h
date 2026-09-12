#pragma once
#include "DrillEngine.h"

#include <mutex>

// Accessed by the server timer and desktop controls, never by the audio thread.
class DrillHistory {
public:
  DrillHistory () {
    records = juce::JSON::parse (file ().loadFileAsString ());
    if (!records.isObject ()) {
      records = juce::var (new juce::DynamicObject ());
    }
  }
  static juce::String key (const juce::String &pattern, int spacing, int kick, double tolerance, double passThreshold) {
    return pattern.removeCharacters (" \t\r\n").toUpperCase () + ":" + juce::String (spacing) + ":" +
           juce::String (kick) + ":" + juce::String (static_cast<int> (std::round (tolerance * 10))) + ":" +
           juce::String (static_cast<int> (std::round (passThreshold * 100)));
  }
  double suggestion (const juce::String &pattern, int spacing, int kick, double tolerance, double passThreshold) const {
    std::lock_guard<std::mutex> lock (mutex);
    const double best =
        static_cast<double> (records[juce::Identifier (key (pattern, spacing, kick, tolerance, passThreshold))]);
    return best > 0       ? std::clamp (std::floor (best * constants::drill::resumeRatio), constants::drill::minBpm,
                                        static_cast<double> (constants::params::bpmMax))
           : spacing == 0 ? constants::drill::eighthStartBpm
                          : constants::drill::startBpm;
  }
  juce::var updateAndCopy (const DrillEngine::Snapshot &s) {
    std::lock_guard<std::mutex> lock (mutex);
    if (s.best != lastRecorded) {
      lastRecorded = s.best;
      if (s.best > 0) {
        const int spacing = s.config.interval == constants::musical::ppq_1_8    ? 0
                            : s.config.interval == constants::musical::ppq_1_8T ? 1
                            : s.config.interval == constants::musical::ppq_1_6  ? 3
                                                                                : 2;
        records.getDynamicObject ()->setProperty (key (juce::String (s.config.pattern.data ()), spacing, s.config.kick,
                                                       s.config.tolerance, s.config.passThreshold),
                                                  s.best);
        file ().getParentDirectory ().createDirectory ();
        file ().replaceWithText (juce::JSON::toString (records));
      }
    }
    return records.clone ();
  }

private:
  static juce::File file () {
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Gridlock/drill-history.json");
  }
  mutable std::mutex mutex;
  juce::var records;
  double lastRecorded{0};
};
