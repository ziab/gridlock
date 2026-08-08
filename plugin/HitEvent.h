#pragma once

#include <cstdint>

enum class TimingState { OnGrid, Rush, Drag };

struct HitEvent {
  uint8_t noteNumber{0};
  uint8_t velocity{0};
  double rawHitPpqPosition{0.0}; // Original uncompensated hit PPQ
  double hitPpqPosition{0.0};    // Compensated hit PPQ
  double deltaMs{0.0};           // Calculated delta ms
  float normalizedDeviation{0.0f};
  TimingState state{TimingState::OnGrid};
};
