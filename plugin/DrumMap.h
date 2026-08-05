#pragma once

#include <cstdint>
#include <juce_core/juce_core.h>
#include <vector>

namespace DrumMap
{
// Standard General MIDI / Roland e-Kit Note Numbers
constexpr uint8_t CymbalEdge29 = 29;    // Roland Cymbal Edge / Crash 2 Rim
constexpr uint8_t CymbalBell30 = 30;    // Roland Cymbal Bell / Splash Rim
constexpr uint8_t Kick = 36;            // C1
constexpr uint8_t SnareHead = 38;       // D1
constexpr uint8_t SnareRim = 40;        // E1
constexpr uint8_t ClosedHiHatEdge = 22; // Roland/Yamaha Closed Edge
constexpr uint8_t ClosedHiHat = 42;     // F#1
constexpr uint8_t PedalHiHat = 44;      // G#1
constexpr uint8_t OpenHiHatEdge = 26;   // Roland/Yamaha Open Edge
constexpr uint8_t OpenHiHat = 46;       // A#1
constexpr uint8_t HighTom = 48;         // C2
constexpr uint8_t MidTom = 45;          // A1
constexpr uint8_t LowTom = 43;          // G1
constexpr uint8_t Crash1 = 49;          // C#2
constexpr uint8_t Ride = 51;            // D#2
constexpr uint8_t ChineseCymbal = 52;   // E2 (Chinese Cymbal)
constexpr uint8_t Crash2 = 53;          // F2
constexpr uint8_t SplashCymbal = 55;    // G2 (Splash Cymbal)
constexpr uint8_t Crash2Edge = 57;      // A2 (Crash 2 Edge)
constexpr uint8_t RideEdge = 59;        // B2 (Ride Edge)

// Test Mode Velocity Synthesis Parameters
namespace TestModeVelocity
{
constexpr uint8_t BaseHiHat = 95;
constexpr int HiHatRange = 15;

constexpr uint8_t BaseKick = 115;
constexpr int KickRange = 10;

constexpr uint8_t BaseSnare = 118;
constexpr int SnareRange = 8;

constexpr uint8_t Crash = 125;
} // namespace TestModeVelocity

struct LaneInfo
{
    juce::String label;
    std::vector<uint8_t> notes;
};

// Velocity-dependent debounce for Note 46 (Open Hi-Hat Tip) ghost-note suppression.
//
// NOTE: This logic is specific to the Roland TD-27 module paired with the Roland VH-14D
// digital hi-hat cymbal. On hard edge hits the VH-14D fires the edge/closed note first,
// then emits a secondary Note 46 (open tip) ~40-60ms later as a hardware artifact.
// This debounce suppresses that spurious retrigger without affecting legitimate open-hat strikes.
//
// Hard open-hat hits arrive loud and legitimately after ~60ms.
// Ghost retriggers are quiet (velocity ~7) and should be suppressed for the full 200ms window.
//
// The curve is quadratic: window = max - (max - min) * (velocity / 127)^2
//   velocity = 127  -> ~60 ms  (loud, accept quickly)
//   velocity =  64  -> ~164 ms
//   velocity =   7  -> ~200 ms (quiet ghost, suppressed nearly always)
static constexpr double kHiHatDebounceMinMs = 60.0;
static constexpr double kHiHatDebounceMaxMs = 200.0;

inline double hiHatDebounceWindowMs (uint8_t velocity) noexcept
{
    const double v = static_cast<double> (velocity) / 127.0; // normalise 0..1
    const double t = v * v;                                  // quadratic - non-linear boost for loud hits
    return kHiHatDebounceMaxMs - (kHiHatDebounceMaxMs - kHiHatDebounceMinMs) * t;
}

// Notes to exclude entirely from the grid display (currently no notes excluded)
inline bool isExcluded (uint8_t note) noexcept
{
    juce::ignoreUnused (note);
    return false;
}

inline const std::vector<LaneInfo> &getStandardDrumLanes ()
{
    static const std::vector<LaneInfo> lanes = {
        {"CYMBALS",
         {CymbalEdge29, CymbalBell30, Crash1, Ride, ChineseCymbal, Crash2, SplashCymbal, Crash2Edge, RideEdge}},
        {"HI-HAT", {ClosedHiHatEdge, ClosedHiHat, PedalHiHat, OpenHiHatEdge, OpenHiHat}},
        {"TOMS", {HighTom, MidTom, LowTom}},
        {"SNARE", {SnareHead, SnareRim}},
        {"KICK", {Kick}},
        {"OTHER", {}}};
    return lanes;
}
} // namespace DrumMap
