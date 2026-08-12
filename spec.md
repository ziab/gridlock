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
| `click_sample_preset`| Click Sound Preset| Choice | Wood Clave, Drum Stick Click, Digital Beep | Wood Clave |
| `click_volume`| Click Volume | Float | 0.0 to 2.0 (+6 dB Boost) | 0.8 |
| `click_pan`   | Click Panning | Float | -1.0 (L) to +1.0 (R) | 0.0 |
| `click_enabled`| Metronome On/Off | Bool | Toggle | True |
| `is_paused`   | Pause/Freeze Grid| Bool | Toggle | False |
| `show_ms_labels`| Display MS Offsets| Bool| Toggle | True |
| `show_velocity_labels`| Display Velocity| Bool| Toggle | False |
| `show_note_numbers`  | Display Note # Labels| Bool| Toggle | False |
| `test_mode`    | Rock Beat Demo Mode| Bool| Toggle / CLI `--test` | False |
| `note_filter`  | Display Mode | Choice | All Notes, Roland/GM Drum Map, Custom | Roland/GM Drum Map |

---

### 4.4a Shared Utilities (`Timing.h`, `Theme.h`, `Crypto.h`)
* `Timing::compute()` (`plugin/Timing.h`) — single source for `deltaMs/norm/state` (was triplicated in processor + test-beat + renderer).
* `Theme` (`plugin/Theme.h`) — central `0xff00ff88`/`0xff38bdf8`/backgrounds; replaces 15+ scattered hex literals.
* `Crypto::sha1()` (`plugin/Crypto.h`) — RFC 3174 SHA-1 extracted from `RemoteControlServer.cpp` for WS handshake `Sec-WebSocket-Accept`.
* `RingBuffer<N>` (`plugin/RingBuffer.h`) — `juce::AbstractFifo` wrapper; capacity `N` holds `N-1` items (one slot gap) — `4096` → `4095`.

### 4.4b Window State Persistence (`PluginEditor.h:110` w)
* `isMaximized` stored in `ApplicationProperties` (`Gridlock/settings.xml` + `PropertiesFile` for standalone) and `APVTS` child `uiState/isMaximized` for DAW reload. Restored in `parentHierarchyChanged` via `DocumentWindow::isFullScreen`, saved on destroy + 60Hz poll. Standalone-only (VST host owns window).

### 4.4 Centralized E-Kit Drum Map & Constants (`Source/DrumMap.h`)
* **Single Source of Truth (`DrumMap.h`)**:
  - Centralized General MIDI / Roland e-Kit constants: `Kick` (36), `SnareHead` (38), `SnareRim` (40), `ClosedHiHat` (42), `PedalHiHat` (44), `OpenHiHat` (46), `HighTom` (48), `MidTom` (45), `LowTom` (43), `Crash1` (49), `Ride` (51), `Crash2` (53), `ChineseCymbal` (52), `SplashCymbal` (55), `Crash2Edge` (57), `CymbalEdge29` (29), `CymbalBell30` (30).
  - Standard drum lane layout `getStandardDrumLanes()` consumed directly by `GridComponent` and `PluginProcessor`. Zero magic note numbers in renderer or audio processor.

---

* **Instant Tolerance Boundary, Vector Symbols & Directional Guidance**:
  - **Within Tolerance ($|\Delta\text{ms}| \le \text{Tolerance}$):** Pure Emerald Green (`#00FF88`) with a bold vector **Checkmark (`✓`)** drawn inside the note circle.
  - **Beyond Tolerance ($|\Delta\text{ms}| > \text{Tolerance}$):**
    - **Rush ($\Delta\text{ms} < 0$):** Vector Right Arrow **`>`** rendered inside note circle (indicating drummer must play *later*).
    - **Drag ($\Delta\text{ms} > 0$):** Vector Left Arrow **`<`** rendered inside note circle (indicating drummer must play *earlier*).
    - *(When `show_velocity_labels` is enabled, velocity integer renders inside instead).*
  - Falloff ratio $t = \text{clamp}\left(\frac{|\Delta\text{ms}| - \text{Tolerance}}{\text{MaxErrorMs} - \text{Tolerance}}, 0.0, 1.0\right)$.
    - **Rush:** Yellow (`#FFEA00`) $\rightarrow$ Orange $\rightarrow$ Electric Red (`#FF1744`).
    - **Drag:** Cyan (`#00E5FF`) $\rightarrow$ Deep Blue $\rightarrow$ Vivid Purple (`#D500F9`).

---

### 4.5b Calibration Wizard (`PluginProcessor.cpp:440` / `GridComponent`+`control_screen.dart:821` w)
* **Flow:** `Idle` → (user `CALIBRATE`) → `CountIn` (1 bar grid-synced to next `timeSigNum` quarters, throne overlay) → `Recording` (4 bars, `expectedHits=round(4*timeSig/interval)` where `interval=getEffectiveGridInterval()` = click `1/4|1/8|1/16|Triplet` if `click_enabled` else `subdivision` `1/8|1/8T|1/16|1/16T|1/32` ) → `Done` (`mean/median/SD`, `failReason` `noHits`/`tooFew` `<50%`/`jitter` `SD>20`), `mean<0` clamped 0. `hitCount` via `nearestGrid` in `[recStart,recEnd)` using `calibLatencyPpq` (`auto+device`, excl. `user`). Trim outliers `>2*SD` for final mean. `Apply` (replace) / `Add` / `Cancel` via `RemoteControlServer` `calibrate`/`calibration_apply`/`calibration_cancel`.
* **Broadcast:** `{"type":"calibration","state":"countin|recording|done","progress":0..1,"beatsRemaining":int,"hitCount":int,"expectedHits":int,"hasResult":bool,"reason":""|"noHits"|"tooFew"|"jitter","meanMs":float,"medianMs":float,"sdMs":float,"bpm":float,"gridInterval":float,"timeSigNum":int}` at 10Hz + Done. Companion `control_screen.dart:848` maps `reason` → short button `NO HITS — RETRY`/`TOO FEW — RETRY`/`UNSTABLE — RETRY` (fits 340px) + subtitle with `hits/exp`/`SD`.
* **E2E:** `companion/bin/calibration_e2e.dart` headless UDP `9877`+WS `9876`, sets `subdivision 1/8` + `click off` → `32/32` `hasResult true`, verifies live `hitCount>0` progress.

### 4.5 Rolling Window Accuracy Score Bar (`GridComponent.cpp`)
* **Accuracy Percentage Calculation**:
  - Evaluated continuously over all visible notes in active history window (`barsWindow`).
  - $\text{Accuracy\%} = \frac{N_{\text{green}}}{N_{\text{total}}} \times 100\%$ ($0\%$ if $N_{\text{total}} = 0$).
* **UI Score Footer**:
  - High-contrast bottom progress bar with Emerald Green fill (`#00FF88`).
  - Bold text readout: `ACCURACY: 88% (22 / 25 Notes On-Grid)`.

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
3. **Remote Control Latency**: WebSocket parameter sync should feel instantaneous (<50 ms). The 10 Hz polling interval for parameter change detection is a tradeoff between responsiveness and CPU overhead.
4. **UDP Broadcast Discovery**: May not work across subnets or on networks that block broadcast traffic. User can fall back to manual IP entry in future versions.

---

## 7. Remote Control Server (`Source/RemoteControlServer.h` / `.cpp`)

### 7.1 Overview
A lightweight network server embedded in the Standalone application that enables companion devices (phones, tablets) on the same LAN to remotely control APVTS parameters. **Active exclusively in Standalone mode** — bypassed when running as VST3/AU plugin.

### 7.2 Discovery Protocol (UDP)

| Field | Value |
| :--- | :--- |
| Transport | UDP Broadcast |
| Port | `9877` |
| Probe Message | `GRIDLOCK_DISCOVER` |
| Response Format | `GRIDLOCK_HERE:<wsPort>` (e.g. `GRIDLOCK_HERE:9876`) |

* The `DiscoveryListenerThread` binds to UDP port `9877` and listens for probe packets.
* On receiving `GRIDLOCK_DISCOVER`, it replies directly to the sender's IP:port with `GRIDLOCK_HERE:9876`.
* Probe interval on the client side: every 500 ms, with a configurable timeout (default 8–10 seconds).

### 7.3 WebSocket Control Protocol

| Field | Value |
| :--- | :--- |
| Transport | WebSocket (RFC 6455, text frames only) |
| Default Port | `9876` |
| Handshake | Standard HTTP Upgrade with SHA-1 accept key |

#### Messages — Server → Client

| Type | Payload | When |
| :--- | :--- | :--- |
| `state` | `{"type":"state","params":{"internal_bpm":{"value":120,"norm":0.308,"name":"Internal BPM","min":40,"max":300,"step":0.1,"paramType":"float"},...}}` | On initial connection |
| `changed` | `{"type":"changed","id":"internal_bpm","value":140.0,"norm":0.385}` | When any parameter changes (10 Hz polling) |

#### Messages — Client → Server

| Type | Payload | Effect |
| :--- | :--- | :--- |
| `set` | `{"type":"set","id":"internal_bpm","value":140.0}` | Calls `param->setValueNotifyingHost()` with denormalized value |
| `get_state` | `{"type":"get_state"}` | Triggers full state snapshot broadcast |

#### Parameter Metadata in State Snapshot

Each parameter object includes:
* `value` — Current denormalized value
* `norm` — Current normalized (0–1) value
* `name` — Human-readable parameter name
* `min`, `max`, `step` — Range info
* `paramType` — One of: `float`, `int`, `bool`, `choice`
* `options` — Array of choice labels (only for `choice` type)

### 7.4 Architecture

```
┌──────────────────────────────────────────────┐
│           RemoteControlServer                │
│                                              │
│  ┌─────────────────┐  ┌──────────────────┐   │
│  │ Discovery Thread │  │ Accept Thread    │   │
│  │ (UDP :9877)      │  │ (WS :9876)       │   │
│  │                  │  │                  │   │
│  │ Listens for      │  │ Accepts WS       │   │
│  │ GRIDLOCK_DISCOVER│  │ connections,     │   │
│  │ → replies with   │  │ sends state      │   │
│  │   WS port        │  │ snapshot         │   │
│  └─────────────────┘  └──────────────────┘   │
│                                              │
│  ┌──────────────────────────────────────┐    │
│  │ Timer Callback (10 Hz)               │    │
│  │ • Read client messages               │    │
│  │ • Detect APVTS parameter changes     │    │
│  │ • Push changed params to clients     │    │
│  └──────────────────────────────────────┘    │
│                                              │
│            ↕ APVTS reference                 │
└──────────────────────────────────────────────┘
```

---

## 8. Testing (`plugin/LogicTests.cpp`, `plugin/RemoteControlServerTests.cpp`)
* **Framework:** `juce::UnitTest` / `UnitTestRunner` (no external gtest). `LogicTests` groups 7 suites under `P1` (`Timing`, `HitColor`, `HiHatDebounce`, `Subdivisions`, `Crypto`, `RingBuffer`, `DrumMapLanes`) via `beginTest`/`expect*`.
* **Integration:** `RemoteControlServerTests` — UDP probe, WS handshake (`s3pPL...`), state/changed frames.
* **Run:** `cmake --build build --config Debug && ./build/LogicTests_artefacts/Debug/LogicTests` (7/7) + `RemoteControlServerTests` (4/4).

## 9. Flutter Companion App (`companion/`)

### 8.1 Overview
A mobile companion app (Android/iOS) that discovers the running Gridlock standalone on the same WiFi network, connects via WebSocket, and provides drum-throne-friendly remote control of metronome and analysis parameters.

### 8.2 Tech Stack

| Component | Technology |
| :--- | :--- |
| Framework | Flutter 3.x (Dart) |
| State Management | Provider + ChangeNotifier |
| Networking | `dart:io` (UDP), `web_socket_channel` (WebSocket) |
| Typography | Google Fonts (Inter) |
| Target Platforms | Android, iOS |

### 8.3 Project Structure

```
companion/lib/
├── main.dart                          # App entry, dark theme, Provider setup
├── models/
│   └── parameter.dart                 # RemoteParameter model (mirrors JUCE JSON)
├── services/
│   ├── discovery_service.dart         # UDP broadcast discovery
│   └── connection_service.dart        # WebSocket client + bidirectional state sync
├── screens/
│   └── control_screen.dart            # Main UI (discovery view + control view)
└── widgets/
    ├── bpm_dial.dart                  # Large rotary BPM dial with arc indicator
    ├── signature_picker.dart          # Segmented time signature chips
    ├── subdivision_picker.dart        # Segmented click subdivision chips
    └── parameter_card.dart            # Reusable slider/toggle/choice card
```

### 8.4 UI Design

#### Control Hierarchy

| Priority | Controls | Widget |
| :--- | :--- | :--- |
| **Primary (Top 55%)** | BPM | `BpmDial` — Rotary dial with arc, drag-to-adjust, tap-to-type |
| **Primary** | Time Signature | `SignaturePicker` — Segmented chips: 2/4, 3/4, 4/4, 5/4, 6/8, 7/8 |
| **Primary** | Click Subdivision | `SubdivisionPicker` — Segmented chips: Off, 1/4, 1/8, 1/16, Triplets |
| **Secondary (Bottom 45%)** | All other params | `ParameterCard` grid — Slider, toggle, or choice variant |

#### Secondary Parameters Exposed

| Parameter ID | Card Label | Type |
| :--- | :--- | :--- |
| `click_enabled` | Metronome | Toggle |
| `is_paused` | Pause | Toggle |
| `click_volume` | Click Volume | Slider |
| `click_pan` | Click Pan | Slider |
| `click_sample_preset` | Click Sound | Choice chips |
| `bars_window` | History Bars | Choice chips |
| `subdivision` | Grid Subdiv | Choice chips |
| `tolerance_ms` | Tolerance | Slider (suffix: ms) |
| `latency_offset_ms` | Latency | Slider (suffix: ms) |
| `min_velocity` | Min Velocity | Slider |
| `show_ms_labels` | MS Offsets | Toggle |
| `note_filter` | Display Mode | Choice chips |

#### Parameters Excluded from Companion App
* `show_velocity_labels` — UI display toggle (not useful from throne distance)
* `show_note_numbers` — UI display toggle
* `test_mode` — Debug/demo mode

#### Theme
* Dark palette matching JUCE app: `#0a0c10` (background), `#181b24` (header), `#141722` (cards)
* Accent: Emerald Green `#00FF88` (primary), Sky Blue `#38bdf8` (secondary)
* Haptic feedback on all interactions
* Portrait-locked for drum throne ergonomics

### 8.5 Connection Flow

1. App launches → **Discovery Screen** with animated Gridlock logo
2. Sends UDP broadcast `GRIDLOCK_DISCOVER` every 500 ms (timeout: 10 s)
3. On response → connects WebSocket to `ws://<ip>:9876`
4. On connect → receives full parameter state → transitions to **Control Screen**
5. Bidirectional sync: changes on phone push to JUCE; changes on JUCE push to phone
6. On disconnect → returns to Discovery Screen with retry option
