# Bot Rules and Tuning

NenoSerpent uses a backend-switched bot runtime with tunable rule strategy config.

## Runtime Architecture

- `adapter/bot/controller.*`: movement and choice scoring.
- `adapter/bot/runtime.*`: state-aware bot flow (`Playing`, `ChoiceSelection`, `GameOver`, etc.).
- `adapter/bot/config.*`: profile loading, default values, build-profile selection.
- `adapter/bot/backend.*`: unified backend abstraction (`decideDirection`, `decideChoice`, `reset`).
- `adapter/bot/ml_backend.*`: lightweight C++ MLP inference backend (JSON weights).

`EngineAdapter` only injects the current game snapshot and executes bot outputs.

Backend behavior (`F8`):

- `off`: bot disabled
- `human`: bot disabled for autoplay, but manual directions are recorded as training samples
- `rule`: rule backend enabled
- `ml`: ML backend enabled; automatic fallback to `rule` on model unavailable/inference miss
- `ml-online`: same inference path as `ml`, plus periodic model-file hot reload while playing
- `search`: depth-limited lookahead search backend (no model dependency)

Strategy behavior (`F10`):

- `safe -> balanced -> aggressive -> safe` cycle
- this only changes rule-policy weights; it does not change backend (`off|human|rule|ml|ml-online|search`)

Startup backend override:

- `NENOSERPENT_BOT_BACKEND=off|human|rule|ml|ml-online|search`
- optional `NENOSERPENT_BOT_ML_MODEL=/abs/path/policy.runtime.json`
- optional human-teach dataset output:
  - `NENOSERPENT_BOT_HUMAN_DATASET=/abs/path/human_dataset.csv`
- optional confidence gate:
  - `NENOSERPENT_BOT_ML_MIN_CONF` (default `0.55`)
  - `NENOSERPENT_BOT_ML_MIN_MARGIN` (default `0.10`)
- optional hot-reload controls (`ml-online` only):
  - `NENOSERPENT_BOT_ML_ONLINE_HOT_RELOAD=1|0` (default `1`)
  - `NENOSERPENT_BOT_ML_ONLINE_RELOAD_TICKS=N` (default `24`)

Strategy behavior:

- `safe`: stronger survivability bias
- `balanced`: default profile behavior
- `aggressive`: stronger scoring/path-shortening bias

## Rule Decision Pipeline (Current)

As of the latest refactor, `rule` and `search` backends use a unified staged pipeline:

1. `Hard Filter`
   - Candidate actions must pass legality plus survival prechecks.
   - Strict filter stats are collected for telemetry:
     - `strict_ok`
     - reject buckets: `safe`, `space`, `tail`
2. `Target FSM`
   - `FoodChase` / `PowerChase` / `Escape`
   - Transition hysteresis is used to avoid per-tick oscillation.
   - Escape scoring branch follows FSM mode directly (not only repeat-threshold guard).
   - Hard escape recovery:
     - prolonged no-score window enables temporary `tail-chase` forcing
     - power-chase is temporarily cooled down after prolonged stalls
3. `Bounded Scoring Blocks`
   - Score composition:
     - `Progress + Survival + Reward - Risk - LoopCost`
   - Each block is range-clamped and normalized to avoid long-run saturation.
   - Escape survival term uses compressed scoring plus stall-decay to reduce late-stage flattening.
   - Under no-score stall windows, target-distance drift shaping is applied:
     - moving farther from primary target gets extra progress penalty
     - moving closer gets bounded progress bonus
4. `Loop Control`
   - Loop observation (`LoopMemory`) is separated from loop penalty/escape policy (`LoopController`).
   - Escape mode applies orbit-break shaping:
     - stall-level-based preferred direction jitter
     - straight/reverse suppression
     - quadratic revisit penalty escalation
     - 2-step/4-step cycle continuation penalty
   - During forced tail-chase window, power reward bias is suppressed.
   - Under prolonged escape stalls, target anchor rotates across board corners to break rings.
   - Under deep stalls, escape scoring switches to survival-only override to force breakout.
   - Deep-stall escape also penalizes short-window direction repetition to avoid orbit lock-in.
5. `Decision Telemetry`
   - Runtime summary includes filter acceptance/reject stats and top-3 candidate score contributions
     (`p/s/r/d/rk/lc`, where `d` is stall drift shaping).

This pipeline is intentionally backend-internal, so adapter/QML does not depend on
scoring internals.

## Strategy Profiles

Built-in profile file:

- `src/adapter/bot/strategy_profiles.json`

Resolution order:

1. `NENOSERPENT_BOT_STRATEGY_FILE` (if set and loadable)
2. Built-in resource profile file
3. Hardcoded defaults in `defaultStrategyConfig()`

Build profile selection key:

- Debug build: `debug`
- RelWithDebInfo build: `dev`
- Release/MinSizeRel build: `release`

## Benchmark

Run with:

```bash
./scripts/dev.sh bot-benchmark --games 300 --max-ticks 5000 --profile dev --mode balanced --backend rule
```

The benchmark reports max/avg/median/p95 score and game-over/timeout outcomes.

Run full reproducible `rule` vs `ml` gate:

```bash
./scripts/dev.sh bot-ml-gate --workspace cache/dev/nenoserpent_bot_ml_gate
```

Online evolution loop (`ml-online` backend + external trainer):

```bash
./scripts/dev.sh bot-online-train --workspace cache/dev/nenoserpent_bot_online
./scripts/dev.sh bot-run --backend ml-online \
  --ml-model cache/dev/nenoserpent_bot_online/nenoserpent_bot_policy_runtime.json --headful
```

Online loop validation gate:

```bash
./scripts/dev.sh bot-ml-online-gate --workspace cache/dev/nenoserpent_bot_ml_online_gate --rounds 3
```

CI regression gate:

```bash
./scripts/ci/bot_e2e_regression.sh build/debug
```

Baseline scenarios live in `scripts/ci/bot_e2e_baseline.tsv` and lock fixed
`backend + mode + level + seed + threshold` tuples.

Leaderboard benchmark suite (fixed 20 scenarios):

```bash
./scripts/dev.sh bot-leaderboard build/debug
```

Suite definition is tracked in `scripts/ci/bot_leaderboard_suite.tsv`.
This is used to compare long-run relative strength across modes and seeds.
When `BOT_ML_MODEL` is set and suite contains `ml` rows, the script emits a
`rule vs ml` compare table (score + choice decision quality) and can enforce no-regression via
`BOT_LEADERBOARD_REQUIRE_NO_REGRESSION=1`.

Extreme-map regression suite:

```bash
./scripts/dev.sh bot-extreme build/dev
```

Gate knobs:

- `BOT_EXTREME_MIN_AVG`
- `BOT_EXTREME_MIN_P95`
- `BOT_EXTREME_MAX_LOOP_RATE`
- `BOT_EXTREME_MAX_TIMEOUT_RATE`

Offline parameter tuning:

```bash
./scripts/dev.sh bot-tune --mode balanced --iterations 60 --output cache/dev/nenoserpent_bot_tuned.json
```

Loop-aware calibration knobs are available:

```bash
./scripts/dev.sh bot-tune --max-loop-rate 0.40 --objective-loop-penalty 75
```

The tuned file can be loaded via:

```bash
NENOSERPENT_BOT_STRATEGY_FILE=cache/dev/nenoserpent_bot_tuned.json ./build/debug/NenoSerpent
```

PyTorch imitation-learning baseline:

- See `docs/BOT_TRAINING.md` for dataset generation, training, and eval workflow.
- Dedicated offline decision evaluators are available for:
  - choice selection (`bot-choice-dataset`, `bot-choice-eval`)
  - power-up chase decisions (`bot-power-dataset`, `bot-power-eval`)

## Live Debug Tuning

Debug tokens accepted by runtime injection:

- `DBG_BOT_PANEL` (toggle panel visibility)
- `DBG_BOT_MODE` (cycle backend: `off -> human -> rule -> ml -> ml-online -> search`)
- `DBG_BOT_STRATEGY` (cycle strategy profile)
- `DBG_BOT_RESET` (reapply current mode defaults)
- `DBG_BOT_PARAM:KEY=VALUE[,KEY=VALUE...]`

Examples:

```text
DBG_BOT_PANEL
DBG_BOT_MODE
DBG_BOT_STRATEGY
DBG_BOT_RESET
DBG_BOT_PARAM:LOOKAHEADWEIGHT=14,LOOKAHEADDEPTH=3,SAFENEIGHBORWEIGHT=16
```

Bot tuner panel now supports mode-preserving rollback via `RESET DEFAULT`,
which reapplies mode defaults (`safe`/`balanced`/`aggressive`) over live tweaks.
