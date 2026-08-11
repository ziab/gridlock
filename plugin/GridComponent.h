#pragma once

#include "Constants.h"
#include "DrumMap.h"
#include "HitEvent.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

struct GridViewState {
  double currentPpq{0.0};
  int numBars{4};
  double gridSubdivisionPpq{constants::musical::ppq_default};
  double effectiveInterval{constants::musical::ppq_default}; // What drummer hears (click if enabled)
  int timeSigNum{constants::params::timeSigDefault};
  bool showMsLabels{true};
  bool showVelocityLabels{false};
  bool showNoteNumbers{false};
  float toleranceMs{constants::params::toleranceDefault};
  float latencyOffsetMs{constants::params::latencyDefault};
  float deviceLatencyMs{0.0f}; // Audio output latency from AudioDeviceManager (Standalone only)
  float bpm{constants::params::bpmDefault};
};

class GridComponent : public juce::Component {
public:
  GridComponent ();
  ~GridComponent () override = default;

  void paint (juce::Graphics &g) override;
  void resized () override;

  void update (const GridViewState &state, const std::vector<HitEvent> &events);
  // Legacy 9-arg overload — delegates to GridViewState version
  void updateEvents (const std::vector<HitEvent> &events, double currentPpq, int numBars, double gridSubdivisionPpq,
                     int timeSigNum = 4, bool showMsLabels = true, bool showVelocityLabels = false,
                     bool showNoteNumbers = false, float toleranceMs = 20.0f, float latencyOffsetMs = 0.0f,
                     float bpm = 120.0f);
  void clearEvents ();

  static juce::Colour getContinuousHitColor (float deltaMs, float toleranceMs, float maxErrorMs) noexcept;

private:
  struct Layout {
    float labelWidth{constants::ui::labelWidth};
    float rulerHeight{constants::ui::rulerHeight};
    float footerHeight{constants::ui::footerHeight};
    float boundsW{0}, boundsH{0};
    float canvasLeft{0}, canvasW{0};
    float laneAreaTop{0}, laneAreaH{0};
    int numLanes{0};
    float laneH{0};
    float dynamicScale{1.0f};
    float nodeRadius{constants::ui::nodeRadiusBase};
    float strokeW{constants::ui::strokeBase};
  };

  Layout computeLayout () const;
  void drawRuler (juce::Graphics &g, const Layout &l) const;
  void drawLanes (juce::Graphics &g, const Layout &l) const;
  void drawGridLines (juce::Graphics &g, const Layout &l, double minPpq, double maxPpq, double totalPpqWindow) const;
  // returns {total, green}
  std::pair<int, int> drawHits (juce::Graphics &g, const Layout &l, double minPpq, double maxPpq, double totalPpqWindow,
                                double totalLatencyPpq, float maxErrorMs) const;
  void drawHitSymbol (juce::Graphics &g, float cx, float cy, float radius, float strokeW, double deltaMs,
                      float absDelta, float tolerance, bool showVel, uint8_t velocity) const;
  void drawPlayhead (juce::Graphics &g, const Layout &l) const;
  void drawFooter (juce::Graphics &g, const Layout &l, int totalHits, int greenHits) const;

  static bool isNearMultiple (double value, double period, double eps = 0.001) noexcept;
  static float lerpChannel (float t, float a, float b) noexcept {
    return juce::jmap (t, a, b);
  }

  std::vector<HitEvent> activeEvents;
  GridViewState view;

  std::vector<DrumMap::LaneInfo> drumLanes;
  int getLaneIndexForNote (uint8_t note) const noexcept;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GridComponent)
};
