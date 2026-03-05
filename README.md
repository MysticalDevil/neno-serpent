# NenoSerpent - Retro GameBoy Style Snake Game (v2.0.0)

[中文版](README_zh-CN.md)

NenoSerpent is a high-quality, cross-platform GameBoy-style Snake game built with **Qt 6** and **C++23**. It faithfully simulates the classic retro handheld experience with modern engineering standards and premium haptic/audio feedback.

Build requirement: **CMake 4.x**.

Preset-first workflow (recommended on CMake 4.x):

```bash
cmake --workflow --preset debug-workflow
```

## Core Features (v2.0.0)

- **GameBoy Boot Flow**: Boot bounce animation + boot beep + delayed BGM handoff to menu.
- **Expanded Navigation**: Hidden fruit library (`LEFT`), achievements room (`UP`), replay (`DOWN`), level switch (`SELECT`).
- **Refined Input Rules**: Context-sensitive `B` behavior restored across menu/game/pause/game-over/roguelike/library/medal.
- **Dynamic Levels**: `Classic`, `The Cage`, `Dynamic Pulse`, `Tunnel Run`, `Crossfire`, `Shifting Box`.
- **Roguelike Power-up Suite**: 9 distinct effects including working **Magnet** fruit attraction and unique portal wall-phasing.
- **Ghost Replay**: Deterministic replay with recorded input and choice playback.
- **Mobile Sensor Glare**: `QtSensors` accelerometer-powered screen reflection movement (with desktop fallback motion).
- **Android Ready**: arm64 deployment pipeline and runtime logcat-driven crash triage workflow.

## Gameplay

- **Core Loop**: eat food, grow longer, and survive as speed increases.
- **Wrap-Around Board**: crossing screen edges loops snake to the opposite side.
- **Level Variants**:
  - `Classic`: no obstacles.
  - `The Cage`: static wall clusters.
  - `Dynamic Pulse`, `Crossfire`, `Shifting Box`: script-driven moving obstacles.
  - `Tunnel Run`: narrow dual-column tunnel pressure.
- **Roguelike Choices**: random ability choices appear as score progresses; each run can evolve differently.
- **Special Fruits**: 9 fruit effects (Ghost/Slow/Magnet/Shield/Portal/Double/Diamond/Laser/Mini) with temporary or instant buffs.
- **Ghost Replay**: best run input+choice replay for route learning and score improvement.

## Tech Stack

- **Language**: C++23 (std::ranges, std::unique_ptr, Coroutines-ready)
- **Framework**: Qt 6.7+ (Quick, JSEngine, Multimedia, Sensors, ShaderTools)
- **Build System**: CMake + Ninja
- **Optional Wrapper**: `zig build` can drive the existing CMake flow

## Project Layout
- Runtime adapter implementation lives in `src/adapter/` (GameLogic split across focused translation units).

## Getting Started

Release notes live in `CHANGELOG.md`.

### Build and Run (Desktop)
```bash
cmake --preset debug
cmake --build --preset debug
./build/debug/NenoSerpent
```

```bash
cmake --preset release
cmake --build --preset release
./build/release/NenoSerpent
```

- `Debug`: detailed runtime logs enabled.
- `RelWithDebInfo` (`dev`): compact runtime logs enabled.
- `Release` / `MinSizeRel`: routine runtime logs are compiled out.

### Build and Run via Zig
```bash
# Debug -> build/debug
zig build

# Release -> build/release
zig build -Doptimize=ReleaseFast

# Build and run
zig build run

# Configure/build debug tests and run ctest from build/debug
zig build test
```

- `zig build` does not call `cmake`; it drives `moc`, `qsb`, `rcc`, `pkg-config`, and `zig c++` directly.
- Override the output profile directory with `-Dprofile=<name>`.
- `zig build test` builds and runs the Qt test executables directly instead of delegating to `ctest`.

### Build and Deploy (Android)
```bash
# Debug build (logs enabled)
CMAKE_BUILD_TYPE=Debug ./scripts/deploy.sh android

# Release build (logs disabled)
CMAKE_BUILD_TYPE=Release ./scripts/deploy.sh android

# Generate Android launcher mipmaps + Play Store 512x512 icon
./scripts/dev.sh android-icons

# Run bot benchmark (builds bot-benchmark target and executes it)
./scripts/dev.sh bot-benchmark --games 300 --max-ticks 5000 --profile dev

# Run fixed bot E2E regression matrix (safe/balanced/aggressive x fixed levels)
./scripts/dev.sh bot-e2e build/debug

# Run fixed 20-case leaderboard benchmark suite
./scripts/dev.sh bot-leaderboard build/debug

# Generate dataset from fixed leaderboard suite (for imitation training)
./scripts/dev.sh bot-dataset --output /tmp/nenoserpent_bot_dataset.csv

# Tune bot profile parameters offline and emit override JSON
./scripts/dev.sh bot-tune --mode balanced --iterations 60 --output /tmp/nenoserpent_bot_tuned.json

# Train and evaluate PyTorch imitation baseline
./scripts/dev.sh bot-train --dataset /tmp/nenoserpent_bot_dataset.csv --model /tmp/nenoserpent_bot_policy.pt
./scripts/dev.sh bot-eval --dataset /tmp/nenoserpent_bot_dataset.csv --model /tmp/nenoserpent_bot_policy.pt

# Reproducible rule-vs-ml full gate (dataset -> train -> eval -> no-regression compare)
./scripts/dev.sh bot-ml-gate --workspace /tmp/nenoserpent_bot_ml_gate

# Quick ml smoke gate with repository tiny model
./scripts/dev.sh bot-ml-smoke build/dev

# Run bot in headful mode directly (no manual F8 cycling needed)
./scripts/dev.sh bot-run --backend rule --headful
./scripts/dev.sh bot-run --backend ml --ml-model /tmp/nenoserpent_bot_policy_runtime.json --headful
```

### Build and Deploy (WebAssembly)
```bash
# Qt WASM toolchain root (example path)
export QT_WASM_PREFIX=~/qt-toolchains/build-qt-wasm/qt-wasm-install-mt

# Build, package to /tmp/neno-serpent-wasm-dist, and serve locally on :8080
./scripts/deploy.sh wasm
```

- Set `SERVE=0` to only build/package without starting a web server.
- Local serving uses `./scripts/deploy.sh wasm-serve` with COOP/COEP headers so `SharedArrayBuffer` works in Chromium-based browsers.
- `qtlogo.svg`/`favicon` are injected from project icon during packaging to keep wasm console/network logs clean.

### Local GitHub CI/CD Simulation (Docker Compose)
```bash
# Put local secrets in .env (this file is git-ignored)
# ANDROID_KEYSTORE_B64=...
# ANDROID_KEYSTORE_PASS=...
# ANDROID_KEY_PASS=...
# ANDROID_KEY_ALIAS=nenoserpent

# Simulate .github/workflows/cmake.yml
docker compose --profile ci run --rm gh-ci

# Simulate .github/workflows/cd.yml (current release + placeholders)
docker compose --profile cd run --rm gh-cd

# Simulate Android CD signing-secret preflight
docker compose --profile cd run --rm gh-cd-android-preflight
```

- Compose uses `docker/ci/Dockerfile` and `scripts/ci/run_gh_sim.sh`.
- The container mounts the current repo at `/workspace` and executes CMake presets directly.

## Controls
- **Arrow Keys**: Move snake
- **START (Enter / S)**: Play / Continue from save
- **SELECT (Shift)**: Cycle levels / (Hold) Delete save
- **UP**: Open Medal Collection
- **DOWN**: Watch Best High-Score Replay
- **LEFT**: Open hidden Fruit Library
- **B / X**:
  - In active game (`Playing` / `Roguelike choice`): switch display palette
  - In menu: switch display palette
  - In pause/game over/replay/library/medal: back to menu
- **Y / C / Tap Logo**: Cycle Console Shell Colors
- **M**: Toggle Music
- **F8**: Cycle Bot Backend (`off -> rule -> ml -> off`)
- **F9**: Toggle Bot Tuning Panel (debug UI)
- **Back / Esc**: Quit App

## Input Architecture Notes
- Logging system plan: `docs/LOGGING_SYSTEM_PLAN.md`
- Audio authoring guide: `docs/AUDIO_AUTHORING.md`
- Level authoring guide: `docs/LEVEL_AUTHORING.md`
- Bot rules and tuning: `docs/BOT_RULES.md`
- Bot training (PyTorch): `docs/BOT_TRAINING.md`
- Bot ML validation gate: `docs/BOT_ML_VALIDATION.md`
- Runtime automation injection: set `NENOSERPENT_INPUT_FILE=/tmp/nenoserpent-input.queue` (recommended) or `NENOSERPENT_INPUT_PIPE=/tmp/nenoserpent-input.pipe`, then send tokens with `./scripts/input.sh inject ...`
- Rule bot strategy override: set `NENOSERPENT_BOT_STRATEGY_FILE=/abs/path/strategy_profiles.json` to override built-in strategy profiles.

## License
Licensed under the [GNU GPL v3](LICENSE).
