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
        {"KICK", {Kick}},
        {"SNARE", {SnareHead, SnareRim}},
        {"TOMS", {HighTom, MidTom, LowTom}},
        {"OTHER", {}}};
    return lanes;
}
} // namespace DrumMap
