#pragma once
#include <cstdint>

enum class TimingState : uint8_t {
    OnGrid, // Within tolerance (+/- ms)
    Rush,   // Played early relative to grid
    Drag    // Played late relative to grid
};

struct HitEvent {
    uint8_t noteNumber{ 0 };
    uint8_t velocity{ 0 };
    double hitPpqPosition{ 0.0 };      // Absolute PPQ timestamp
    double deltaMs{ 0.0 };             // Time diff in ms from nearest grid point
    float normalizedDeviation{ 0.0f }; // Deviation ratio (-1.0 = Max Rush, 0.0 = OnGrid, +1.0 = Max Drag)
    TimingState state{ TimingState::OnGrid };
};
