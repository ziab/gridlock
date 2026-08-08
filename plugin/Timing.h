#pragma once

#include "HitEvent.h"

#include <algorithm>
#include <cmath>

namespace Timing {

struct Result {
  double deltaMs{0.0};
  float normalizedDeviation{0.0f};
  TimingState state{TimingState::OnGrid};
};

inline Result compute (double compensatedPpq, double gridInterval, double bpm, float toleranceMs) noexcept {
  const double nearestGridPpq = std::round (compensatedPpq / gridInterval) * gridInterval;
  const double deltaPpq = compensatedPpq - nearestGridPpq;
  const double deltaMs = (deltaPpq / (bpm / 60.0)) * 1000.0;
  const float normalizedDev =
      std::clamp (static_cast<float> (deltaMs / static_cast<double> (toleranceMs)), -1.0f, 1.0f);

  TimingState state = TimingState::OnGrid;
  if (std::abs (deltaMs) <= static_cast<double> (toleranceMs)) {
    state = TimingState::OnGrid;
  } else if (deltaMs < -static_cast<double> (toleranceMs)) {
    state = TimingState::Rush;
  } else {
    state = TimingState::Drag;
  }

  return {deltaMs, normalizedDev, state};
}

} // namespace Timing
