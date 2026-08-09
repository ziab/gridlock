# Gridlock — Agent Guidelines

Pragmatic, not perfectionist. Performance second. Keep `spec.md` truthful, keep style consistent, keep tests green.

## General

- **Spec first:** `spec.md:1-354` is design source. If you change behavior/architecture, update `spec.md` compactly (add `w`/`m` note, don't rewrite).
- **Build = truth:** `cmake --build build --config Debug --parallel 8` must pass before review. C++ tests `build/LogicTests_artefacts/Debug/LogicTests.exe` (7/7) + `RemoteControlServerTests` (4/4). Companion `flutter analyze --no-pub` (0 issues) + `flutter test` (9/9).
- **No commit without review** for style/magic-number sweeps. Show `git diff --stat`.
- **Indentation:** 2 spaces (`TabWidth: 2` `.clang-format:6`, `.editorconfig:15/18`). Single-stance braces `if (...) {` on same line (`BreakBeforeBraces: Attach` `.clang-format:19`).

## C++ (`plugin/` — JUCE 7, C++20)

**Style — enforced by linter:**
- `clang-format -i --style=file plugin/*.h plugin/*.cpp` with `.clang-format` (LLVM, `Attach`, `AllowShortBlocks: Never`, `ColumnLimit: 120`).
- **Always braced** — no `if/for/while/else` without `{}` (`readability-braces-around-statements`). Even `if (x) return;` → `if (x) { return; }`. Linter doesn't auto-insert — manual pass required.
- Checked via `Select-String "^\s*(if|for|while)\s*\(.*\)\s*$" | ? {$_ -notmatch "\{"}` → 0.

**Architecture — don't repeat these mistakes:**

- **Duplicated timing math:** was triplicated `PluginProcessor.cpp:310` vs `:370` vs `GridComponent.cpp:265`. Now single `Timing::compute(compPpq, grid, bpm, tol)` `plugin/Timing.h:13` → `HitEvent` fields `plugin/HitEvent.h:13`. New code must use it; add `plugin/LogicTests.cpp:27` case.
- **God functions:** `GridComponent::paint:96` was 345 lines. Now `computeLayout/drawRuler/drawLanes/drawGridLines/drawHits/drawPlayhead/drawFooter` `plugin/GridComponent.cpp:106` + `GridComponent.h:40`. Same for `PluginEditor::PluginEditor:7` (236 lines) → `setupControls/attachParameters/setupTimeSigHandling` + `FlexBox` `resized:334`. Don't add new monoliths.
- **Param boilerplate:** was 77-line `createParameterLayout:45`. Now `addChoice/addFloat/addInt/addBool` helpers + `ParamSnapshot` `readSnapshot()` `plugin/PluginProcessor.h:14`. Use them.
- **Stateful predicate:** `shouldFilterHiHatTrigger:263` mutated `lastOtherHiHatTimeMs:270`. Renamed `updateHiHatHistoryAndShouldFilter:210` — name side-effects.
- **Test-mode lambdas:** `generateTestModeBeat:345` had 2 capturing lambdas (98 lines). Now `generateHumanizedDeviationMs:265` + `makeQuantizedHit:279` + `emit` lambda. Keep helpers small/testable.
- **Click engine split:** `ClickGenerator::renderBlock:153` was 101 lines (schedule+mix+tanh). Now `scheduleClicks/mixActiveVoices/applySoftClipper` `plugin/ClickGenerator.h:36`. Negative `sampleOffset` trick documented.
- **Crypto inline 90 lines:** SHA-1 in `RemoteControlServer.cpp:267` → `Crypto::sha1()` `plugin/Crypto.h:9`. Don't re-inline.
- **Discovery thread** as local class in `.cpp` — keep isolated, testable only via integration.

**Magic numbers — centralized (`plugin/Constants.h:1`):**
- `params::{tolerance 5-40/0.5, latency -500..500, bpm 40-300, timeSig 2-12, clickVol 0-2, pan -1..1, sampleRateFallback 44100}` wired `PluginProcessor.cpp:60` + `ClickGenerator.cpp:10`.
- `musical::{ppq_1_8 0.5, _1_8T, _1_16 0.25, _1_16T, _1_32 0.125, _1_64 0.0625, candidatesAuto}` wired `PluginProcessor::getSubdivisionPpq:87`, `ClickGenerator::getClickSubdivisionPpq:123`, `AsciiTabRenderer.h:104` — don't re-literal.
- `network::{wsPort 9876/udpPort 9877, pollHz 10, wsAccept 200, udpSleep 50, wsMaxPayload 65536}` wired `RemoteControlServer.h:25`/`RemoteControlServer.cpp:38`.
- `ui::{labelWidth 110, ruler 24, footer 26, canvas 1200, lane 80, zoom 1.4/2.0, node 11, ascii 7/4096}` wired `GridComponent.h:42`/`GridComponent.cpp:118`.
- **Keep local:** `kEps = 1e-9` `AsciiTabRenderer.h:32` (ppq compare) vs `0.001` `GridComponent.cpp:199` — different domains, stay local. DSP gains `1.3/0.55` `ClickGenerator.cpp:55` — single-use.

**Other:**
- `Theme.h:9` central `0xff00ff88` etc. — no scattered hex.
- `RingBuffer<N>` holds `N-1` (`juce_AbstractFifo.cpp:29` `freeSpace-1`) — `plugin/RingBuffer.h:12` must check `size1||size2`. Test expects `4096→4095` `plugin/LogicTests.cpp:194`.
- `GridViewState` `GridComponent.h:9` replaces 11-arg `updateEvents:32`. Prefer structs for >3 params.
- Public mutable `internalPpqPosition` etc. privatized `PluginProcessor.h:131` — use getters.

**Testing (`juce::UnitTest`):**
- `LogicTests.cpp:264` 7 suites `Timing/HitColor/HiHatDebounce/Subdivisions/Crypto/RingBuffer/DrumMapLanes` `juce::UnitTest:33` `beginTest/expectWithinAbsoluteError/expectEquals` + `UnitTestRunner:320` (not `gtest`). Keep `RemoteControlServerTests.cpp:82` for networking.

## Dart / Flutter (`companion/` — Dart 3, Provider)

**Theme/constants — mirrors C++:**
- `constants/app_colors.dart:6` `AppColors.bgMain 0xFF0a0c10` etc. — replaces 15× `Color(0xFF…)` `main.dart:24` `control_screen.dart:106`. Always use it.
- `constants/app_constants.dart:6` `udpPort/wsPort/probe 500ms/timeout 8/10s/bpm 30-300/pixelsPerBpm 12/practice 300s/historyOptions` — sync with `constants::network`. Don't re-literal ports `9877/9876` `discovery_service.dart:9` or `127.0.0.1:9876` `control_screen.dart:121`.
- `constants/app_theme.dart:5` `AppTheme.dark/systemUiOverlay` — replaces `ThemeData` `main.dart:44`.

**Structure — don't make a god widget:**
- `control_screen.dart:1367` → split `widgets/discovery_view.dart:7` `DiscoveryView`, `widgets/status_bar.dart:6` `StatusBar`, `widgets/segmented_picker.dart:7` generic `SegmentedPicker(title,options,selected,accent)` extracts 90% dupe `signature_picker.dart:109` vs `subdivision_picker.dart:100` → thin wrappers `signature_picker.dart:6`/`subdivision_picker.dart:6` (15 LOC). `control_screen.dart` is composition now.
- `utils/net_utils.dart:5` `parseHostPort/cleanIp` dedups `connection_service.dart:32` + `control_screen.dart:336` manual `split(':')`.
- Keep `kEps`-like locals local; don't globalize single-use `LinearGradient stops [0.0,0.14…]`.

**Typing & state:**
- Avoid `dynamic` params `control_screen.dart:303` `bpm/timeSig` → `RemoteParameter?` `control_screen.dart:516,609,744` `Map<String,RemoteParameter>`. Use `param.value/min/max`.
- `BpmRulerSelector:15` defaults `30/300/12.0` → `AppConstants.bpmRulerMin/Max/pixelsPerBpm`.
- `PracticeTimerService:8` defaults `300/120→160` → `AppConstants.practiceDefaultSec`.

**Tooling:** `flutter analyze --no-pub` must be 0, `flutter test` 9/9 `companion/test/*`.

## Checklist for next agent

- [ ] `clang-format -i` + brace check → 0 unbracketed
- [ ] No new `0xff…`/`9876`/`0.5` literal — use `Theme/Constants/AppColors`
- [ ] God function < 80 lines or split
- [ ] New timing/bpm math uses `Timing::compute` / `musical` constants
- [ ] `cmake --build` + both test suites + `flutter analyze/test` green
- [ ] `spec.md` updated compactly if architecture changes
- [ ] `git diff --stat` shown, no push without review
