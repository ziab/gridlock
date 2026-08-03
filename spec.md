# Software Design Document: MIDI Grid Analyzer VST3/AU Plugin

## 1. Overview
A real-time MIDI monitoring VST3/AU plugin designed for Ableton Live and other DAWs. The plugin sits in front of a virtual instrument (e.g., Superior Drummer 3), passes incoming MIDI through with zero latency, and captures note timing against the host DAW transport grid. It visualizes the last X bars of played notes, color-coding hits based on timing precision (On-grid, Rush, Drag).

---

## 2. Tech Stack & Requirements

* **Framework:** JUCE 7+ (C++20)
* **Plugin Formats:** VST3, AU (Standalone optional for testing)
* **Target OS:** macOS 12+ / Windows 10+ (64-bit)
* **Key JUCE Modules:** `juce_audio_processors`, `juce_audio_basics`, `juce_gui_basics`, `juce_graphics`

---

## 3. High-Level Architecture

```
                       ┌─────────────────────────┐
                       │  Host DAW (Ableton)     │
                       └────────────┬────────────┘
                                    │ AudioPlayHead (BPM, PPQ, Transport)
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│                        MIDI Grid Analyzer VST3                         │
│                                                                        │
│  ┌────────────────────────┐         Lock-Free FIFO Queue              │
│  │     Audio Thread       │ ─────────────────────────────────┐        │
│  │  (processBlock)        │  HitEvent {note, ppqOffset, ms}  │        │
│  └───────────┬────────────┘                                  │        │
│              │                                               ▼        │
│              │ Pass-through                           ┌─────────────┐ │
│              ▼                                        │ GUI Thread  │ │
│  ┌────────────────────────┐                           │ (Timer/2D)  │ │
│  │ MIDI Output (to SD3)   │                           └─────────────┘ │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 4. System Components & Data Flow

### 4.1 Data Structures (`HitEvent.h`)

```cpp
#pragma once
#include <cstdint>

enum class TimingState : uint8_t {
    OnGrid, // Within tolerance (+/- ms or %)
    Rush,   // Played early relative to grid
    Drag    // Played late relative to grid
};

struct HitEvent {
    uint8_t noteNumber{ 0 };
    uint8_t velocity{ 0 };
    double hitPpqPosition{ 0.0 };  // Absolute PPQ timestamp
    double deltaMs{ 0.0 };         // Time diff in ms from nearest grid point
    TimingState state{ TimingState::OnGrid };
};
```

---

### 4.2 Audio Processor (`PluginProcessor.h` / `.cpp`)

#### Core Responsibilities
1. **Pass-Through:** Copy incoming `juce::MidiBuffer` directly to outgoing `juce::MidiBuffer`.
2. **Host Sync:** Extract BPM, PPQ position, and transport status via `getPlayHead()->getPosition()`.
3. **Sub-block Timing Precision:** Calculate exact PPQ hit timestamp using buffer sample offset:
   Hit PPQ = Current PPQ + (Sample Offset * (BPM / 60) / Sample Rate)
4. **Grid Alignment & Delta Calculation:**
   * Determine current subdivision (e.g., 1/16th note = 0.25 PPQ).
   * Calculate nearest grid division: Grid PPQ = round(Hit PPQ / Subdivision) * Subdivision.
   * Calculate Delta PPQ = Hit PPQ - Grid PPQ.
   * Convert Delta PPQ to milliseconds:
     Delta ms = (Delta PPQ / (BPM / 60)) * 1000
5. **Lock-Free Push:** Push `HitEvent` to a lock-free single-producer single-consumer ring buffer (`juce::AbstractFifo` backed array). *Do not allocate memory or use mutexes in `processBlock()`.*

---

### 4.3 Data Pipeline & Ring Buffer (`RingBuffer.h`)

* Maintain a fixed rolling window of events corresponding to X bars (user-configurable: 1, 2, 4, or 8 bars).
* When total events exceed the buffer length or fall outside the current X-bar rolling PPQ window, evict oldest events on the GUI thread.

---

### 4.4 Plugin Parameters (`juce::AudioProcessorValueTreeState`)

| Parameter ID | Name | Type | Range / Options | Default |
| :--- | :--- | :--- | :--- | :--- |
| `bars_window` | History Length | Choice | 1 Bar, 2 Bars, 4 Bars, 8 Bars | 4 Bars |
| `subdivision` | Grid Subdivision | Choice | 1/8, 1/8T, 1/16, 1/16T, 1/32 | 1/16 |
| `tolerance_ms`| Timing Tolerance | Float | 2.0 ms to 30.0 ms | 10.0 ms |
| `note_filter`  | Display Mode | Choice | All Notes, Kick/Snare/HH, Custom | All Notes |

---

### 4.5 GUI Component (`PluginEditor.h` / `.cpp`)

#### UI Layout & Visual Encoding
* **Dimensions:** Minimum 800x400 px, resizable.
* **Refresh Rate:** `juce::Timer` running at ~60 Hz to poll the FIFO queue.
* **Main Canvas (Grid Display):**
  * **Y-Axis:** Lanes for distinct drum note triggers (e.g., Lane 1: Kick `36`, Lane 2: Snare `38`, Lane 3: Closed HH `42`).
  * **X-Axis:** Rolling PPQ window representing X bars, with vertical subdivision lines.
  * **Playhead:** Vertical line tracking `position->getPpqPosition()`.
* **Note Color Mapping:**
  * **Green:** |Delta ms| <= Tolerance
  * **Red (Rush):** Delta ms < -Tolerance
  * **Purple (Drag):** Delta ms > +Tolerance
* **Controls Header:** Dropdowns for Bars, Subdivision, Tolerance Slider, and Clear Buffer button.

---

## 5. Implementation Steps for Coding Agent

### Step 1: Project Scaffolding
Create a new JUCE audio plugin project named `MidiGridAnalyzer` configured as a **MIDI Effect / VST3 / AU** with no audio inputs/outputs and standard MIDI input/output flags set in Projucer or CMake.

### Step 2: Processor Thread Engine Implementation
Implement lock-free FIFO queue and `processBlock` timing calculation logic. Ensure `juce::MidiBuffer` messages pass through unmodified to the destination buffer.

### Step 3: Rolling Buffer Management
Implement GUI-side event collector that pulls from the FIFO and populates a vector of active `HitEvent` instances scaled to the current PPQ window.

### Step 4: Visual Rendering
Build custom `juce::Component` for grid rendering using `juce::Graphics`. Implement color-coded circle rendering at calculated (X, Y) relative coordinates.

### Step 5: APVTS Parameter Bindings
Wire parameters (`bars_window`, `subdivision`, `tolerance_ms`) to UI controls via `juce::AudioProcessorValueTreeState::ComboBoxAttachment` and `SliderAttachment`.

---

## 6. Edge Cases & Constraints

1. **Host Transport Stopped:** If transport is paused, render static last-recorded events or clear display based on user preference.
2. **BPM Changes:** Recalculate Delta ms dynamically per block using the latest BPM reading from `AudioPlayHead`.
3. **MIDI Map Mappings:** Default MIDI Note mappings should align with GM Drum Standard and Roland VAD/TD-series defaults (Kick: 36, Snare Head: 38, Snare Rim: 40, Closed HH: 42, Open HH: 46).
