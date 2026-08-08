#pragma once

#include <juce_graphics/juce_graphics.h>

namespace Theme {
// ── Backgrounds ──
inline constexpr juce::uint32 bgMain = 0xff0a0c10;
inline constexpr juce::uint32 bgHeader = 0xff181b24;
inline constexpr juce::uint32 bgLaneEven = 0xff161922;
inline constexpr juce::uint32 bgLaneOdd = 0xff1a1d28;
inline constexpr juce::uint32 bgSidebar = 0xff12141a;
inline constexpr juce::uint32 bgGrid = 0xff12141a;
inline constexpr juce::uint32 bgProgressTrack = 0xff111827;

// ── Accents ──
inline constexpr juce::uint32 emerald = 0xff00ff88;
inline constexpr juce::uint32 skyBlue = 0xff38bdf8;
inline constexpr juce::uint32 cyan = 0xff00e5ff;
inline constexpr juce::uint32 amber = 0xffeab308;

// ── Borders / lines ──
inline constexpr juce::uint32 border = 0xff2d3245;
inline constexpr juce::uint32 borderFaint = 0xff1e293b;
inline constexpr juce::uint32 borderTrack = 0xff374151;

// ── Text ──
inline constexpr juce::uint32 textPrimary = 0xfff0f2f8;
inline constexpr juce::uint32 textMuted = 0xff94a3b8;
inline constexpr juce::uint32 textLabel = 0xffb0b8c8;
inline constexpr juce::uint32 textOnEmerald = 0xff0a0c10;

// ── Hit colours (interpolated, endpoints only) ──
inline constexpr juce::uint32 rushYellow = 0xffffea00;
inline constexpr juce::uint32 rushRed = 0xffff1744;
inline constexpr juce::uint32 dragCyan = 0xff00e5ff;
inline constexpr juce::uint32 dragPurple = 0xffd500f9;

// ── Button states ──
inline constexpr juce::uint32 buttonIdle = 0xff2d3245;
inline constexpr juce::uint32 buttonClickOn = 0xff00c853;
inline constexpr juce::uint32 buttonPauseOn = 0xffeab308;
inline constexpr juce::uint32 buttonMsOn = 0xff0284c7;
inline constexpr juce::uint32 buttonVelOn = 0xffa855f7;
inline constexpr juce::uint32 buttonNoteOn = 0xff06b6d4;
inline constexpr juce::uint32 buttonTestOn = 0xffec4899;

inline juce::Colour col (juce::uint32 argb) {
  return juce::Colour (argb);
}
} // namespace Theme
