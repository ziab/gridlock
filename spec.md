# Software Design Document: MIDI Grid Analyzer VST3/AU/Standalone Plugin

## 1. Overview
A real-time MIDI monitoring VST3/AU plugin (with Standalone application support) designed for Ableton Live, other DAWs, or standalone e-kit practice. The plugin sits in front of a virtual instrument (e.g., Superior Drummer 3) or e-kit input, passes incoming MIDI through with zero latency, and captures note timing against the host DAW transport grid or internal clock. It visualizes the last X bars of played notes, color-coding hits based on timing precision (On-grid, Rush, Drag).

### 1.1 Primary Users & Core Pain Point
* **Primary Users:** Drummers playing electronic drum kits (e.g., Roland VAD / TD series, Yamaha DTX, EFNOTE) triggering high-end virtual drum instruments like Superior Drummer 3.
* **Core Pain Point:** Traditional DAW grid views require stopping, stepping away from the drum throne, and using a mouse/keyboard to manually inspect recorded MIDI notes for micro-timing issues.

### 1.2 E-Kit Workflow Requirements
1. **Instant Visual Feedback:** The UI must be highly readable from 4–6 feet away (from the drum throne looking at a laptop or secondary monitor). High-contrast UI elements, bold color-coding, and clean lane separation are critical.
2. **Roland / General MIDI Drum Map Defaults:** By default, vertical lanes should automatically map to standard e-kit MIDI note layouts:
   * **Kick:** Note `36`
   * **Snare Head / Rim:** Notes `38` / `40`
   * **Hi-Hat (Closed / Open / Pedal):** Notes `42` / `46` / `44`
   * **Toms (Rack 1, 2, Floor):** Notes `48`, `45`, `43`
   * **Cymbals (Crash / Ride / Ride Bell):** Notes `49`, `51`, `53`
3. **Sensitivity & Crosstalk Tolerance:** E-kits can occasionally output false micro-velocity triggers or rapid double-triggering. The timing parser should ignore velocity values below a customizable noise floor (e.g., Velocity < 5).
4. **Standalone App Support (Secondary Goal):** Support building as a native standalone desktop app (macOS / Windows) for quick practice sessions without launching a DAW. The architecture must gracefully handle transport/clock state in both plugin and standalone modes.

---

## 2. Tech Stack & Requirements

* **Framework:** JUCE 7+ (C++20)
* **Plugin Formats:** VST3, AU, Standalone Application (`FORMATS VST3 AU Standalone`)
* **Target OS:** macOS 12+ / Windows 10+ (64-bit)
* **Key JUCE Modules:** `juce_audio_processors`, `juce_audio_basics`, `juce_gui_basics`, `juce_graphics`, `juce_audio_utils` (for Standalone device setup)

---

## 3. High-Level Architecture

```
                       ┌─────────────────────────┐
                       │  Host DAW / Standalone  │
                       └────────────┬────────────┘
                                    │ AudioPlayHead (BPM, PPQ, Transport)
                                    │ OR Internal Clock Generator (Fallback)
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│                     MIDI Grid Analyzer (VST3/AU/App)                   │
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
2. **Host Sync & Standalone Clock Fallback:**
   * Query `getPlayHead()->getPosition()`.
   * If running in a DAW, extract BPM, PPQ position, and transport status from the host playhead.
   * If running as a **Standalone app** or when host playhead is unavailable/stopped, fallback to an **Internal Clock Generator** that advances internal PPQ position using sample rate and user-specified `internal_bpm`.
3. **Crosstalk & Noise Floor Filtering:** Ignore Note-On messages with `velocity < min_velocity` threshold to filter out e-kit crosstalk and false triggers.
4. **Sub-block Timing Precision:** Calculate exact PPQ hit timestamp using buffer sample offset:
   Hit PPQ = Current PPQ + (Sample Offset * (BPM / 60) / Sample Rate)
5. **Grid Alignment & Delta Calculation:**
   * Determine current subdivision (e.g., 1/16th note = 0.25 PPQ).
   * Calculate nearest grid division: Grid PPQ = round(Hit PPQ / Subdivision) * Subdivision.
   * Calculate Delta PPQ = Hit PPQ - Grid PPQ.
   * Convert Delta PPQ to milliseconds:
     Delta ms = (Delta PPQ / (BPM / 60)) * 1000
6. **Lock-Free Push:** Push `HitEvent` to a lock-free single-producer single-consumer ring buffer (`juce::AbstractFifo` backed array). *Do not allocate memory or use mutexes in `processBlock()`.*

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
| `min_velocity`| Velocity Noise Floor | Int | 1 to 127 | 5 |
| `internal_bpm`| Internal BPM | Float | 40.0 to 300.0 BPM | 120.0 BPM |
| `note_filter`  | Display Mode | Choice | All Notes, Roland/GM Drum Map, Custom | Roland/GM Drum Map |

---

### 4.5 GUI Component (`PluginEditor.h` / `.cpp`)

#### UI Layout & Visual Encoding
* **Dimensions:** Minimum 800x400 px, resizable.
* **Visibility & Ergonomics:** High-contrast Dark Mode theme optimized for throne-distance viewing (4–6 feet away) with large indicators, bold typography, and crisp visual separation.
* **Refresh Rate:** `juce::Timer` running at ~60 Hz to poll the FIFO queue.
* **Main Canvas (Grid Display):**
  * **Y-Axis:** Vertical lanes for Roland / GM drum note triggers:
    * Kick (`36`)
    * Snare Head (`38`) & Rim (`40`)
    * Hi-Hat Closed (`42`), Open (`46`), Pedal (`44`)
    * Toms: Rack 1 (`48`), Rack 2 (`45`), Floor (`43`)
    * Cymbals: Crash (`49`), Ride (`51`), Ride Bell (`53`)
  * **X-Axis:** Rolling PPQ window representing X bars, with vertical subdivision lines.
  * **Playhead:** Vertical line tracking PPQ position (Host or Internal Clock).
* **Note Color Mapping:**
  * **Green (On-Grid):** |Delta ms| <= Tolerance
  * **Red (Rush):** Delta ms < -Tolerance
  * **Purple (Drag):** Drag ms > +Tolerance
* **Controls Header:** Dropdowns for Bars, Subdivision, Tolerance Slider, Velocity Threshold Slider, Internal BPM Slider (visible/active in Standalone/Internal mode), and Clear Buffer button.

---

## 5. Implementation Steps for Coding Agent

### Step 1: Project Scaffolding
Create a new JUCE audio project named `MidiGridAnalyzer` configured as a **MIDI Effect / VST3 / AU / Standalone Application** (`FORMATS VST3 AU Standalone`) with no audio inputs/outputs and standard MIDI input/output flags set in Projucer or CMake.

### Step 2: Processor Thread Engine Implementation
Implement lock-free FIFO queue, internal fallback clock generator for Standalone mode, and `processBlock` timing calculation logic with velocity noise floor filtering (`min_velocity`). Ensure `juce::MidiBuffer` messages pass through unmodified to the destination buffer.

### Step 3: Rolling Buffer Management
Implement GUI-side event collector that pulls from the FIFO and populates a vector of active `HitEvent` instances scaled to the current PPQ window.

### Step 4: Visual Rendering
Build custom `juce::Component` for grid rendering using `juce::Graphics`. Implement color-coded circle rendering at calculated (X, Y) relative coordinates with high-contrast UI design tailored for throne distance (4–6 ft).

### Step 5: APVTS Parameter Bindings
Wire parameters (`bars_window`, `subdivision`, `tolerance_ms`, `min_velocity`, `internal_bpm`) to UI controls via `juce::AudioProcessorValueTreeState::ComboBoxAttachment` and `SliderAttachment`.

---

## 6. Edge Cases & Constraints

1. **Host Transport Stopped / Standalone Mode:** When running in Standalone mode or if host transport is paused without playhead info, switch automatically to the internal clock generator driven by `internal_bpm` so real-time practice analysis continues seamlessly.
2. **BPM Changes:** Recalculate Delta ms dynamically per block using the active BPM (Host or Internal).
3. **MIDI Drum Maps & Noise Floor:** Default MIDI Note mappings correspond to Roland VAD/TD series & General MIDI defaults (Kick 36, Snare 38/40, HH 42/44/46, Toms 48/45/43, Cymbals 49/51/53). Velocity noise floor prevents crosstalk artifacts from polluting the grid.


