# Software Design Document: MIDI Grid Analyzer Standalone App & VST3/AU Plugin

## 1. Overview
A real-time MIDI analyzer and drum practice application available as a Standalone Desktop Application (Primary Use Case) and VST3/AU Plugin. Designed for drummers playing electronic drum kits (Roland, Yamaha, EFNOTE) or triggering virtual instruments (Superior Drummer 3). The application provides zero-latency MIDI pass-through, a built-in sample-based metronome click generator with time signatures & click subdivisions, and instant throne-distance micro-timing visual analysis (color-coded hits across rolling PPQ bars).

### 1.1 Primary Users & Core Pain Point
* **Primary Users:** Drummers playing electronic drum kits (e.g., Roland VAD / TD series, Yamaha DTX, EFNOTE) practicing micro-timing or triggering high-end virtual drum instruments.
* **Core Pain Point:** Traditional DAW grid views require stopping, stepping away from the drum throne, and using a mouse/keyboard to manually inspect recorded MIDI notes for micro-timing issues.

### 1.2 Core Workflow Requirements
1. **Throne-Distance Visual Feedback:** Highly readable UI from 4–6 feet away with high-contrast elements, bold typography, clean drum lane separation, and smooth continuous color gradients for hits.
2. **Roland / General MIDI Drum Map Defaults:** **Main Canvas (Grid Display):**
  * **Y-Axis (Top to Bottom):** Vertical drum lanes ordered as:
    1. **Cymbals:** Crash (`49`), Ride (`51`), Ride Bell (`53`)
    2. **Hi-Hat:** Closed (`42`), Open (`46`), Pedal (`44`)
    3. **Kick:** Note (`36`)
    4. **Snare:** Head (`38`) & Rim (`40`)
    5. **Toms:** Rack 1 (`48`), Rack 2 (`45`), Floor (`43`)
    6. **Other:** Unmapped MIDI notes
  * **X-Axis:** Rolling PPQ window representing X bars, with top timeline ruler and explicit visual markers for Bar boundaries (Bar 1, Bar 2) and Strong Beats (1, 2, 3, 4).
  * **Playhead:** Vertical line tracking PPQ position (Host or Internal Clock).
3. **Sensitivity & Crosstalk Noise Floor:** Ignore velocity values below customizable noise floor (`min_velocity`, e.g. Velocity < 5) to prevent false triggers.
4. **Built-in Sample-Based Metronome Click Engine (Standalone Mode Only):**
   * Active **exclusively in Standalone Application mode** (bypassed when running as a VST3/AU plugin in a DAW, where host DAW click is used).
   * High quality sample playback for accent (Beat 1), beat, and subdivision clicks.
   * Configurable Time Signature (e.g. 4/4, 3/4, 6/8, 5/4, 7/8).
   * Configurable Click Subdivisions (Off, 1/4, 1/8, 1/16, Triplets).
   * Independent Click Volume & Mute control (`click_volume`, `click_enabled`).
5. **Continuous Dynamic Color Gradient:** Hit color continuously interpolates based on time offset relative to tolerance ($ ratio = \Delta\text{ms} / \text{Tolerance} $):
   * **On-Grid ($ ratio = 0 $):** Pure Emerald Green (`#00FF88`).
   * **Rush / Early ($ ratio < 0 $):** Smooth transition Green $\rightarrow$ Yellow $\rightarrow$ Orange $\rightarrow$ Electric Red (`#FF1744`).
   * **Drag / Late ($ ratio > 0 $):** Smooth transition Green $\rightarrow$ Cyan $\rightarrow$ Blue/Violet $\rightarrow$ Vivid Purple (`#D500F9`).

---

## 2. Tech Stack & Requirements

* **Framework:** JUCE 7+ (C++20)
* **Plugin / App Formats:** Standalone Application (Primary), VST3, AU
* **Audio Outputs:** Stereo Audio Output for Metronome Click Generation
* **Target OS:** macOS 12+ / Windows 10+ (64-bit)
* **Key JUCE Modules:** `juce_audio_processors`, `juce_audio_basics`, `juce_audio_utils`, `juce_gui_basics`, `juce_graphics`, `juce_audio_formats`

---

## 3. High-Level Architecture

```
                       ┌─────────────────────────┐
                       │   Standalone / Host     │
                       └────────────┬────────────┘
                                    │ AudioPlayHead (BPM, Time Signature, PPQ)
                                    │ OR Internal Clock & Metronome Engine
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│                        MIDI Grid Analyzer App                          │
│                                                                        │
│  ┌────────────────────────┐         Lock-Free FIFO Queue              │
│  │     Audio Thread       │ ─────────────────────────────────┐        │
│  │  (processBlock)        │  HitEvent {note, ppqOffset, ms}  │        │
│  │  + Click Sample Player │                                  │        │
│  └───────────┬────────────┘                                  │        │
│              │                                               ▼        │
│    ┌─────────┴─────────┐                              ┌─────────────┐ │
│    ▼                   ▼                              │ GUI Thread  │ │
│ MIDI Pass-Through  Stereo Audio Click Output          │ (Timer/2D)  │ │
│ (to VST/e-kit)     (to Speakers/Headphones)           └─────────────┘ │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 4. System Components & Data Flow

### 4.1 Data Structures (`HitEvent.h`)

```cpp
#pragma once
#include <cstdint>

enum class TimingState : uint8_t {
    OnGrid,
    Rush,
    Drag
};

struct HitEvent {
    uint8_t noteNumber{ 0 };
    uint8_t velocity{ 0 };
    double hitPpqPosition{ 0.0 };  // Absolute PPQ timestamp
    double deltaMs{ 0.0 };         // Time diff in ms from nearest grid point
    float normalizedDeviation{ 0.0f }; // Continuous ratio (-1.0 = Max Rush, 0.0 = OnGrid, +1.0 = Max Drag)
    TimingState state{ TimingState::OnGrid };
};
```

---

### 4.2 Audio Processor & Click Generator (`PluginProcessor.h` / `.cpp`, `ClickGenerator.h`)

#### Core Responsibilities
1. **Audio Output:** Stereo audio output enabled to output metronome click sample audio.
2. **Sample-Based Click Generator:**
   * High-quality sample buffers for Downbeat (High), Beat (Mid), and Subdivision (Low) clicks.
   * Trigger sample playback accurately at sample-exact offsets in `processBlock` based on BPM, Time Signature, and Click Subdivision settings.
3. **MIDI Pass-Through & Crosstalk Filtering:** Pass MIDI through with zero latency while discarding Note-On events with `velocity < min_velocity`.
4. **Sub-block Timing Precision & Live Real-Time Latency Compensation:**
   * Query total latency (`autoLatencyMs` + `latency_offset_ms`).
   * Store `rawHitPpqPosition` on each `HitEvent`.
   * Dynamically calculate `compensatedHitPpq`, `deltaMs`, `normalizedDeviation`, and color gradients in real time during canvas rendering (`GridComponent::paint`).
   * Dragging the `Latency` or `Tolerance` sliders instantly shifts and updates all visible notes on the grid in real time.
5. **Lock-Free Push:** Push `HitEvent` to `RingBuffer` for GUI rendering.

---

### 4.3 Plugin Parameters (`juce::AudioProcessorValueTreeState`)

| Parameter ID | Name | Type | Range / Options | Default |
| :--- | :--- | :--- | :--- | :--- |
| `bars_window` | History Length | Choice | 1 Bar, 2 Bars, 4 Bars, 8 Bars | 4 Bars |
| `subdivision` | Grid Subdivision | Choice | 1/8, 1/8T, 1/16, 1/16T, 1/32 | 1/16 |
| `tolerance_ms`| Timing Tolerance | Float | 0.0 ms to 100.0 ms | 20.0 ms |
| `latency_offset_ms`| Latency Compensation | Float | -500.0 ms to 500.0 ms | 0.0 ms |
| `min_velocity`| Velocity Noise Floor | Int | 1 to 127 | 5 |
| `internal_bpm`| Metronome BPM | Float | 40.0 to 300.0 BPM | 120.0 BPM |
| `time_sig_num`| Time Sig Numerator | Int | 2 to 12 | 4 |
| `click_subdivision`| Click Subdivision | Choice | Off, 1/4 Notes, 1/8 Notes, 1/16 Notes, Triplets | 1/4 Notes |
| `click_sample_preset`| Click Sound Preset| Choice | Wood Clave, Drum Stick Click, Digital Beep, Cowbell | Wood Clave |
| `click_volume`| Click Volume | Float | 0.0 to 1.0 (Linear gain) | 0.8 |
| `click_pan`   | Click Panning | Float | -1.0 (L) to +1.0 (R) | 0.0 |
| `click_enabled`| Metronome On/Off | Bool | Toggle | True |
| `is_paused`   | Pause/Freeze Grid| Bool | Toggle | False |
| `show_ms_labels`| Display MS Offsets| Bool| Toggle | True |
| `note_filter`  | Display Mode | Choice | All Notes, Roland/GM Drum Map, Custom | Roland/GM Drum Map |

---

### 4.4 GUI Component & Continuous Color Mapping (`GridComponent.h` / `.cpp`)

* **Tolerance & Falloff Color Gradient Algorithm**:
  - `MaxErrorMs` $= (\text{subdivisionPpq} / 2.0) \times (60.0 / \text{BPM}) \times 1000.0$.
  - **Within Tolerance ($|\Delta\text{ms}| \le \text{Tolerance}$):** Pure Emerald Green (`#00FF88`).
  - **Beyond Tolerance ($|\Delta\text{ms}| > \text{Tolerance}$):** Falloff ratio $t = \text{clamp}\left(\frac{|\Delta\text{ms}| - \text{Tolerance}}{\text{MaxErrorMs} - \text{Tolerance}}, 0.0, 1.0\right)$.
    - **Rush ($\Delta\text{ms} < 0$):** Smooth transition Green $\rightarrow$ Yellow $\rightarrow$ Orange $\rightarrow$ Electric Red (`#FF1744`) as $t \rightarrow 1.0$.
    - **Drag ($\Delta\text{ms} > 0$):** Smooth transition Green $\rightarrow$ Cyan $\rightarrow$ Blue/Violet $\rightarrow$ Vivid Purple (`#D500F9`) as $t \rightarrow 1.0$.

---

## 5. Implementation Steps for Coding Agent

### Step 1: Click Generator Engine & Audio Outputs
Implement sample-based `ClickGenerator` class. Update `PluginProcessor` constructor to enable stereo audio output bus. Wire metronome sample triggering to sample offsets in `processBlock`.

### Step 2: Time Signature & Click Subdivision Parameters
Add `time_sig_num`, `click_subdivision`, `click_volume`, and `click_enabled` parameters to APVTS layout and processor state.

### Step 3: Continuous Color Gradient Rendering
Update `HitEvent` structure to record `normalizedDeviation`. Update `GridComponent::paint` to render hit nodes using a smooth HSL/RGB gradient function ranging from Red (Rush) to Green (On-grid) to Purple (Drag).

### Step 4: UI Control Header Expansion
Add controls to `PluginEditor` for Metronome Toggle, Click Volume, Time Signature, and Click Subdivision.

---

## 6. Edge Cases & Constraints

1. **Standalone Metronome Timing**: Metronome click triggers must maintain sample-exact alignment with grid PPQ positions without drift over long practice sessions.
2. **Audio Buffer Mixing**: Metronome click audio must sum cleanly into output channels without clipping or pop artifacts.



