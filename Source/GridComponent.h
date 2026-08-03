#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include "HitEvent.h"

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
                      bool showMsLabels = true);
    void clearEvents();

    static juce::Colour getContinuousHitColor(float normalizedDeviation) noexcept;

private:
    struct DrumLaneInfo {
        juce::String label;
        juce::Array<uint8_t> notes;
    };

    std::vector<HitEvent> activeEvents;
    double currentPpqPos{ 0.0 };
    int barsWindow{ 4 };
    double subdivisionPpq{ 0.25 };
    int timeSigNumerator{ 4 };
    bool displayMsLabels{ true };

    std::vector<DrumLaneInfo> drumLanes;
    int getLaneIndexForNote(uint8_t note) const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GridComponent)
};
