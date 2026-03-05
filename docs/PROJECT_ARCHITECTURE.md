# NenoSerpent Project Architecture

Last updated: 2026-03-06

## 1. Architecture Goals

- Keep gameplay rules deterministic and testable.
- Keep UI composition independent from gameplay simulation details.
- Keep platform-specific runtime concerns (audio/sensors/haptics/persistence) isolated in adapter/services.
- Keep bot/ML features pluggable and non-invasive to core gameplay.

## 2. Layered Structure (Current)

### 2.1 Core Domain (`src/core`)

Core owns game rules and deterministic state transitions.

- `core/session/*`: `SessionCore`, `SessionRunner`, tick pipeline, replay timeline integration.
- `core/game/*`: collision/wrap/rule primitives.
- `core/buff/*`: buff runtime rules.
- `core/choice/*`: choice generation and buff selection model.
- `core/level/*`: built-in level fallback/runtime materialization.
- `core/replay/*`: replay frame/choice timeline application.
- `core/achievement/*`: achievement rule evaluation.

Key property:
- No QML dependency.
- No platform dependency.
- Logic is unit-test friendly.

### 2.2 FSM (`src/fsm`)

FSM owns high-level app state orchestration.

- `fsm/states.cpp`
- `fsm/state_factory.cpp`

Key property:
- State transition policy is centralized.
- `EngineAdapter` delegates transition orchestration to FSM callbacks.

### 2.3 Adapter (`src/adapter`)

Adapter bridges core/fsm/services to UI-facing API.

Main entry:
- `adapter/engine_adapter.h/.cpp`

Split modules:
- `adapter/session/*`: runtime reset/level state helpers.
- `adapter/input/*`: input routing and semantics.
- `adapter/ui/*`: UI command/controller glue.
- `adapter/view_models/*`: render/status/selection/theme/audio view models.
- `adapter/level/*`: level loading/script runtime/application.
- `adapter/bot/*`: rule/search/ml/ml-online/human backends, runtime facade, telemetry.
- `adapter/haptics/*`: haptics control.
- `adapter/achievement/*`, `adapter/ghost/*`, `adapter/models/*`, `adapter/profile/*`.

Key property:
- Concentrates integration complexity.
- Exposes QML-facing properties/signals while preserving core determinism.

### 2.4 Services (`src/services`)

Infrastructure services with narrow responsibilities:

- `services/audio/bus.cpp`: typed audio-event routing and policy layer.
- `services/level/repository.cpp`: level resource loading.
- `services/save/repository.cpp`: persistence backend.

### 2.5 Audio Runtime (`src/audio` + `src/sound_manager.*`)

- `audio/event.h`: typed audio intent.
- `audio/score.*`: score catalog parsing and resource loading.
- `sound_manager.*`: synthesis/playback runtime and track execution.

Current shape:
- Event/policy split is already present.
- Runtime playback still terminates at monolithic `SoundManager`.

### 2.6 UI/QML (`src/qml`)

Composition-centric UI:

- `main.qml` + `UiCompositionHost.qml`: top-level host.
- `ScreenView.qml`: screen stack owner and layer composition.
- Modal/overlay/static debug surfaces and reusable controls.
- Input/debug controllers (`UiInputController.qml`, `UiDebugController.qml`, etc.).

Key property:
- UI state mostly sourced from view models and controller ports.
- Static debug injection is script-addressable and documented.

### 2.7 Entry and Wiring (`src/main.cpp`)

Bootstrap wiring:

- Instantiate `EngineAdapter`, view models, `SoundManager`.
- Connect adapter audio signals to sound runtime.
- Register QML types/context objects.
- Load `qrc:/src/qml/main.qml`.

## 3. Runtime Data Flow

### 3.1 Gameplay Tick

1. Input/UI action enters adapter (`adapter/input/router.cpp`).
2. `EngineAdapter` drives simulation tick (`adapter/tick_driver.cpp`, `adapter/simulation.cpp`).
3. Core step executes (`SessionCore` / `SessionRunner`).
4. Adapter emits property/signal updates.
5. View models propagate to QML render tree.
6. Audio/haptic events are emitted through typed event path.

### 3.2 Bot Tick

1. Snapshot builder extracts runtime snapshot from adapter/core state.
2. Backend (`rule`/`search`/`ml`/`ml-online`) produces direction/choice decision.
3. Decision applier mutates runtime queue/choice index through narrow applier hooks.
4. Telemetry logs route/fallback details.

### 3.3 Audio

1. Gameplay/UI emits `audio::Event`.
2. `AudioBus` resolves cue/ducking/music policy.
3. `EngineAdapter` forwards final playback signals.
4. `SoundManager` executes synthesis/playback.

## 4. What Is Already Strong

- Core gameplay and replay are deterministic and test-driven.
- Bot subsystem is mostly isolated under `adapter/bot`.
- Logging categories and mode policy (`release/dev/debug`) are established.
- Script entrypoints are centralized (`scripts/dev.sh`, `scripts/ui.sh`, `scripts/input.sh`, `scripts/deploy.sh`).
- Static debug UI injection surface is broad and practical.

## 5. Optimization Backlog (Actionable)

This section is intentionally prioritized by architectural leverage.

### P0: Reduce `EngineAdapter` God-object Surface

Current issue:
- `EngineAdapter` still owns too many cross-domain responsibilities (tick orchestration, state glue,
  persistence, input routing, audio forwarding, bot control, debug injection hooks).

Action:
- Continue splitting by explicit ports/components:
  - `SessionPort` (simulation step, replay, level transitions)
  - `InputPort` (semantic action mapping and dispatch)
  - `AudioPort` (event dispatch only)
  - `DebugInjectPort` (all DBG token and static seed hooks)
- Keep `EngineAdapter` as coordinator shell, not implementation host.

Expected outcome:
- Smaller public API.
- Easier targeted tests.
- Lower regression blast radius.

### P1: Finish Audio System Phase 5/6

Current issue:
- User BGM import is partial.
- Advanced mixer behavior is partial.

Action:
- Complete user-facing import UX + metadata validation policy.
- Add per-group gain and richer ducking transitions.

Expected outcome:
- Audio extensibility without touching gameplay code.
- Cleaner policy boundary.

### P1: Bot Decision Quality Pipeline Upgrade

Current issue:
- Direction quality is gated; choice/power decision gates are new and should be stabilized in CI.

Action:
- Keep `choice` + `power-up` datasets/eval fully integrated with `bot-ml-gate`.
- Add CI lanes for decision-quality no-regression.
- Add model metadata/provenance contract for publish artifacts.

Expected outcome:
- ML updates won’t silently regress choice/power behavior.

### P2: Resource Schema Governance

Current issue:
- Level/audio/theme/bot resource validation is still mostly runtime fallback based.

Action:
- Add JSON schema or validator tooling in CI for:
  - `audio/score_catalog.json`
  - `levels/levels.json`
  - bot strategy profile JSON
  - theme/config payloads

Expected outcome:
- Invalid assets fail fast before packaging.

### P2: UI Regression Baseline Productization

Current issue:
- Capture tooling exists, but baseline diff policy and thresholded pass/fail are incomplete.

Action:
- Add baseline image store + diff generation + threshold policy.
- Make UI regression output machine-readable in CI artifacts.

Expected outcome:
- UI regressions become objective and reviewable.

## 6. Suggested Refactor Sequence

1. Shrink `EngineAdapter` by extracting `InputPort` + `DebugInjectPort`.
2. Stabilize bot decision quality gates in CI (`choice` + `power`).
3. Complete audio Phase 5/6.
4. Add resource schema validation.
5. Add UI regression baseline diff gate.

## 7. Reference Map

- Core: [src/core](/home/omega/ai-workspace/gameboy-snack/src/core)
- Adapter: [src/adapter](/home/omega/ai-workspace/gameboy-snack/src/adapter)
- Services: [src/services](/home/omega/ai-workspace/gameboy-snack/src/services)
- Audio: [src/audio](/home/omega/ai-workspace/gameboy-snack/src/audio)
- QML UI: [src/qml](/home/omega/ai-workspace/gameboy-snack/src/qml)
- FSM: [src/fsm](/home/omega/ai-workspace/gameboy-snack/src/fsm)
- Modernization roadmap: [docs/MODERNIZATION_ROADMAP.md](/home/omega/ai-workspace/gameboy-snack/docs/MODERNIZATION_ROADMAP.md)
- Audio plan: [docs/AUDIO_SYSTEM_PLAN.md](/home/omega/ai-workspace/gameboy-snack/docs/AUDIO_SYSTEM_PLAN.md)
- Bot rules: [docs/BOT_RULES.md](/home/omega/ai-workspace/gameboy-snack/docs/BOT_RULES.md)
