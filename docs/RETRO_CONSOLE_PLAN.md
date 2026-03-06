# Retro Console Expansion Plan

Last updated: 2026-03-07

## Purpose

This document defines the long-term plan for evolving NenoSerpent from a single-game retro handheld into a small multi-game retro console.

Target direction:

1. one shared console shell
2. multiple game modules
3. shared meta systems such as achievements, library, replay, audio, and UI themes

The intended first three playable modules are:

1. Snake
2. Breakout
3. Tetris

This is a structural plan, not a commitment to implement every phase immediately.

## Current Assessment

The project already has a strong reusable shell, but the gameplay runtime is still tightly coupled to Snake.

### What is already reusable

- shell and menu presentation
- QML theme and icon system
- state-machine driven screen flow
- audio shell and retro presentation
- achievements, library, and debug injection concepts
- replay and session framing patterns

Representative files:

- [src/game_engine_interface.h](/home/omega/ai-workspace/gameboy-snack/src/game_engine_interface.h)
- [src/fsm/states.cpp](/home/omega/ai-workspace/gameboy-snack/src/fsm/states.cpp)
- [src/qml/ScreenView.qml](/home/omega/ai-workspace/gameboy-snack/src/qml/ScreenView.qml)
- [src/qml/OverlayLayer.qml](/home/omega/ai-workspace/gameboy-snack/src/qml/OverlayLayer.qml)
- [src/qml/icons](/home/omega/ai-workspace/gameboy-snack/src/qml/icons)
- [src/qml/meta](/home/omega/ai-workspace/gameboy-snack/src/qml/meta)

### What is still Snake-specific

- engine interface vocabulary
- world render model
- session core semantics
- game rules and power-up model
- bot runtime
- library naming and data assumptions

Representative files:

- [src/game_engine_interface.h](/home/omega/ai-workspace/gameboy-snack/src/game_engine_interface.h)
- [src/adapter/view_models/render.h](/home/omega/ai-workspace/gameboy-snack/src/adapter/view_models/render.h)
- [src/core/game](/home/omega/ai-workspace/gameboy-snack/src/core/game)
- [src/core/session](/home/omega/ai-workspace/gameboy-snack/src/core/session)
- [src/adapter/session](/home/omega/ai-workspace/gameboy-snack/src/adapter/session)
- [src/adapter/bot](/home/omega/ai-workspace/gameboy-snack/src/adapter/bot)

### Bottom line

Today the project is best described as:

- `console shell`: mostly reusable
- `game runtime`: still a Snake runtime

That means the project can become a retro console, but only after a deliberate platformization pass.

## Non-Goals

This plan does not aim to:

- bolt three games onto the current Snake runtime with conditionals
- make every existing gameplay-specific concept generic immediately
- preserve old internal Snake-oriented naming forever
- introduce a plugin system in the first wave

The immediate goal is not "support infinite games". The goal is "support 2 to 3 games cleanly".

## Target Architecture

The target architecture should have three explicit layers.

### Layer 1: Console Shell

The console shell owns the retro device experience.

Responsibilities:

- boot flow
- top-level menu
- game selection
- shell audio and presentation
- persistent profile
- achievements room
- library / icon lab / debug gallery
- shared input routing
- per-game launch and return flow

This layer should not know Snake rules, Tetris rotation, or Breakout collisions.

### Layer 2: Game Runtime Interface

This is the most important missing layer.

It should replace the current Snake-oriented runtime surface with a small, stable interface that every game module can implement.

Suggested responsibilities:

- identify active game
- advance one frame/tick
- accept abstract input
- expose a generic board or scene render model
- expose game-specific HUD data through a stable schema
- expose per-game library and achievement descriptors
- expose pause, replay, save, and game-over state

It must not hard-code concepts such as:

- `snakeModel`
- `fruitLibrary`
- `powerUpType`

### Layer 3: Game Modules

Each game should own its own rules, runtime, render adapter, and optional bot.

Initial modules:

1. `snake`
2. `breakout`
3. `tetris`

Each module should contain:

- game rules
- frame/session runtime
- adapter from core state to shared render model
- module-local achievements and library content
- tests and benchmarks relevant to that module

## Recommended Terminology Shift

The current naming exposes the main structural problem directly.

Examples that should stop being top-level shared concepts:

- `snakeModel`
- `fruitLibrary`
- `powerUp`
- `medalRoom` as if medals were globally tied to a single game

Recommended shared vocabulary:

- `gameId`
- `gameRuntime`
- `boardViewModel` or `playfieldViewModel`
- `gameLibraryModel`
- `achievementCatalog`
- `gameChoiceModel` only where the current game actually supports choice mechanics

Snake-specific names should stay inside the Snake module, not at the global shell boundary.

## Why Breakout Before Tetris

If the project expands beyond Snake, Breakout should come first.

Reasons:

1. Breakout is structurally closer to Snake than Tetris is
- single active playfield
- real-time movement
- simple immediate input
- HUD and game-over semantics fit the current shell better

2. Breakout forces useful generalization
- playfield rendering
- collision reporting
- score and lives handling
- stage progression

3. Tetris will force deeper abstraction
- piece queue
- hold slot
- rotation system
- line clear animation
- gravity lock timing

That makes Tetris a better third step, after the shared runtime interface is proven.

## Migration Phases

### Phase 1: Shell and Snake Separation

Goal:

- split "console shell" from "Snake runtime"

Tasks:

- rename shared interfaces to remove Snake-first vocabulary
- isolate Snake-only code under clearer module boundaries
- make top-level QML screens depend on generic render/view-model contracts

Acceptance:

- top-level shell code no longer references `snakeModel`
- Snake remains playable with unchanged behavior

### Phase 2: Introduce a Generic Game Runtime Interface

Goal:

- create a stable host/runtime contract for multiple games

Tasks:

- replace direct `EngineAdapter == Snake engine` assumptions
- define a generic runtime interface for tick/input/state/render/hud
- move per-game data translation into module-local adapters

Acceptance:

- the shell can launch a runtime without knowing whether it is Snake or another game
- game-specific render state is translated below the shell layer

### Phase 3: Split Shared Meta Systems From Snake Content

Goal:

- make achievements, library, and profile systems multi-game aware

Tasks:

- support per-game achievement catalogs
- support per-game library/catalog content
- namespace unlocks and progress by `gameId`
- separate shell-level achievements from game-level achievements if needed

Acceptance:

- adding a second game does not require hijacking Snake library structures
- profile storage no longer assumes a single game catalog

### Phase 4: Generalize the Render Path

Goal:

- stop treating the world layer as a Snake scene

Tasks:

- split shared LCD/shell/overlay rendering from game playfield rendering
- define one generic playfield entry point in QML
- move Snake drawing into a module-specific layer

Acceptance:

- `WorldLayer` no longer acts as the mandatory rendering path for all future games
- the shell can host different playfield renderers

### Phase 5: Add Breakout

Goal:

- prove the architecture with a second game

Tasks:

- create `core/breakout` and matching adapter/view-model layers
- add menu entry and game launch flow
- add Breakout achievements and library descriptors
- add the smallest useful replay/save support

Acceptance:

- Snake and Breakout both run inside the same shell
- profile, menu, achievements, and overlays all remain coherent

### Phase 6: Add Tetris

Goal:

- validate that the architecture supports a more structurally different game

Tasks:

- create Tetris runtime and render adapter
- add piece queue, hold, line clear presentation, and stage progression
- integrate Tetris-specific achievements and library content

Acceptance:

- Tetris runs without leaking Tetris-only concepts into the shared shell contracts

## Shared Systems Strategy

### Achievements

Achievements should become game-aware.

Recommended model:

- keep a global achievement room
- store achievements as `(gameId, achievementId)`
- optionally reserve a small set of shell/global achievements

Do not keep assuming a single flat list belonging to one game.

### Library / Icon Lab

The library should become catalog-driven.

Recommended model:

- one global shell entry
- multiple catalog tabs or pages by game
- shared icon system
- per-game content descriptors

This allows Snake fruits, Breakout brick/power icons, and Tetris piece/bonus icons to coexist cleanly.

### Replay

Replay support should remain module-local behind a shared shell affordance.

Recommended model:

- shell knows whether replay is available
- each module defines its replay payload and playback rules

### Audio

The audio shell can remain shared, but soundtrack and cue routing should become game-aware.

Recommended model:

- shell events remain shared
- gameplay cues are namespaced per game
- each game can expose its own score/cue suite

This aligns with [AUDIO_SYSTEM_PLAN.md](/home/omega/ai-workspace/gameboy-snack/docs/AUDIO_SYSTEM_PLAN.md).

## Directory Direction

The target structure does not need to be adopted in one shot, but this is the intended direction:

```text
src/
  shell/
    menu/
    profile/
    achievements/
    library/
    ui/
  runtime/
    host/
    contracts/
  games/
    snake/
      core/
      adapter/
      qml/
    breakout/
      core/
      adapter/
      qml/
    tetris/
      core/
      adapter/
      qml/
```

The exact folder names can differ, but the architectural split should match this shape.

## Main Risks

### 1. Conditional-sprawl failure mode

The biggest risk is implementing multi-game support with:

- `if (gameType == Snake)`
- `else if (gameType == Breakout)`
- `else if (gameType == Tetris)`

across shell, adapter, and QML.

That would make the project harder to maintain than it is today.

### 2. Premature over-generalization

The opposite failure is building a giant abstract framework before a second game exists.

The correct path is:

1. define a thin generic host contract
2. prove it with Breakout
3. expand only where Tetris actually requires more structure

### 3. UI shell leakage

If game-specific rendering or HUD assumptions stay embedded in top-level QML, the shell will remain Snake-shaped even after some C++ refactors.

`ScreenView.qml` and the playfield layer boundary need careful control.

## Recommended Immediate Next Step

Do not start by writing Breakout or Tetris directly.

Do this first:

1. write a detailed runtime-host interface proposal
2. list all Snake-specific assumptions currently exposed at the shell boundary
3. choose the minimum set of shared render/HUD contracts
4. then refactor Snake to sit behind that interface

Only after Snake runs cleanly behind the new host contract should the second game be added.

## Success Criteria

This plan should be considered successful when:

1. the shell can launch at least two structurally different games
2. shared UI layers no longer expose Snake-only concepts
3. achievements and library content are namespaced by game
4. adding a third game does not require rewriting the shell
5. gameplay-specific logic stays inside its own module
