#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include "HitEvent.h"
#include "DrumMap.h"

class GridComponent : public juce::Component
{
public:
    GridComponent();
    ~GridComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void updateEvents(const std::vector<HitEvent>& events,
                      double currentPpq,
                      int numBars,
                      double gridSubdivisionPpq,
                      int timeSigNum = 4,
                      bool showMsLabels = true,
                      bool showVelocityLabels = false,
                      bool showNoteNumbers = false,
                      float toleranceMs = 20.0f,
                      float latencyOffsetMs = 0.0f,
                      float bpm = 120.0f);
    void clearEvents();

    static juce::Colour getContinuousHitColor(float deltaMs, float toleranceMs, float maxErrorMs) noexcept;

private:
    std::vector<HitEvent> activeEvents;
    double currentPpqPos{ 0.0 };
    int barsWindow{ 4 };
    double subdivisionPpq{ 0.25 };
    int timeSigNumerator{ 4 };
    bool displayMsLabels{ true };
    bool displayVelLabels{ false };
    bool displayNoteNumLabels{ false };
    float toleranceMsVal{ 20.0f };
    float latencyOffsetMsVal{ 0.0f };
    float bpmVal{ 120.0f };

    std::vector<DrumMap::LaneInfo> drumLanes;
    int getLaneIndexForNote(uint8_t note) const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GridComponent)
};
