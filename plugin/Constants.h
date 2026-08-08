#pragma once

#include <array>

namespace constants {

// ── Parameter ranges (APVTS) ──
namespace params {
constexpr float toleranceMin = 5.0f;
constexpr float toleranceMax = 40.0f;
constexpr float toleranceStep = 0.5f;
constexpr float toleranceDefault = 20.0f;

constexpr float latencyMin = -500.0f;
constexpr float latencyMax = 500.0f;
constexpr float latencyStep = 1.0f;
constexpr float latencyDefault = 0.0f;

constexpr int velMin = 1;
constexpr int velMax = 127;
constexpr int velDefault = 5;

constexpr float bpmMin = 40.0f;
constexpr float bpmMax = 300.0f;
constexpr float bpmStep = 0.1f;
constexpr float bpmDefault = 120.0f;

constexpr int timeSigMin = 2;
constexpr int timeSigMax = 12;
constexpr int timeSigDefault = 4;

constexpr float clickVolMin = 0.0f;
constexpr float clickVolMax = 2.0f;
constexpr float clickVolStep = 0.01f;
constexpr float clickVolDefault = 0.8f;

constexpr float panMin = -1.0f;
constexpr float panMax = 1.0f;
constexpr float panStep = 0.05f;
constexpr float panDefault = 0.0f;

constexpr float sampleRateFallback = 44100.0f;
} // namespace params

// ── Musical PPQ ──
namespace musical {
constexpr double ppq_1_8 = 0.5;
constexpr double ppq_1_8T = 0.5 * 2.0 / 3.0;
constexpr double ppq_1_16 = 0.25;
constexpr double ppq_1_16T = 0.25 * 2.0 / 3.0;
constexpr double ppq_1_32 = 0.125;
constexpr double ppq_1_64 = 0.0625;
constexpr double ppq_default = ppq_1_16;

constexpr double ppq_click_1_4 = 1.0;
constexpr double ppq_click_1_8 = 0.5;
constexpr double ppq_click_1_16 = 0.25;
constexpr double ppq_click_triplet = 1.0 / 3.0;

constexpr std::array<double, 3> candidatesAuto = {ppq_1_16, ppq_1_32, ppq_1_64};
} // namespace musical

// ── Network ──
namespace network {
constexpr int wsPort = 9876;
constexpr int udpPort = 9877;
constexpr int pollHz = 10; // APVTS poll + WS read
constexpr int wsAcceptTimeoutMs = 200;
constexpr int udpSleepMs = 50;
constexpr size_t wsMaxPayload = 65536;
constexpr int clientReadyTimeoutMs = 0; // non-blocking poll
} // namespace network

// ── UI layout (GridComponent + AsciiTab) ──
namespace ui {
constexpr float labelWidth = 110.0f;
constexpr float rulerHeight = 24.0f;
constexpr float footerHeight = 26.0f;
constexpr float canvasRefWidth = 1200.0f;
constexpr float laneHeightRef = 80.0f;
constexpr float zoomScale_2Bars = 1.4f;
constexpr float zoomScale_1Bar = 2.0f;
constexpr float nodeRadiusBase = 11.0f;
constexpr float strokeBase = 2.5f;
constexpr float laneFontBase = 16.0f;

constexpr int asciiLabelWidth = 7;
constexpr int asciiMaxCols = 4096;
} // namespace ui

} // namespace constants
