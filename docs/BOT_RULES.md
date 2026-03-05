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
- `rule`: rule backend enabled
- `ml`: ML backend enabled; automatic fallback to `rule` on model unavailable/inference miss

Startup backend override:

- `NENOSERPENT_BOT_BACKEND=off|rule|ml`
- optional `NENOSERPENT_BOT_ML_MODEL=/abs/path/policy.runtime.json`
- optional confidence gate:
  - `NENOSERPENT_BOT_ML_MIN_CONF` (default `0.90`)
  - `NENOSERPENT_BOT_ML_MIN_MARGIN` (default `1.20`)

Strategy behavior:

- `safe`: stronger survivability bias
- `balanced`: default profile behavior
- `aggressive`: stronger scoring/path-shortening bias

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
./scripts/dev.sh bot-ml-gate --workspace /tmp/nenoserpent_bot_ml_gate
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
`rule vs ml` compare table and can enforce no-regression via
`BOT_LEADERBOARD_REQUIRE_NO_REGRESSION=1`.

Offline parameter tuning:

```bash
./scripts/dev.sh bot-tune --mode balanced --iterations 60 --output /tmp/nenoserpent_bot_tuned.json
```

The tuned file can be loaded via:

```bash
NENOSERPENT_BOT_STRATEGY_FILE=/tmp/nenoserpent_bot_tuned.json ./build/debug/NenoSerpent
```

PyTorch imitation-learning baseline:

- See `docs/BOT_TRAINING.md` for dataset generation, training, and eval workflow.

## Live Debug Tuning

Debug tokens accepted by runtime injection:

- `DBG_BOT_PANEL` (toggle panel visibility)
- `DBG_BOT_MODE` (cycle mode)
- `DBG_BOT_RESET` (reapply current mode defaults)
- `DBG_BOT_PARAM:KEY=VALUE[,KEY=VALUE...]`

Examples:

```text
DBG_BOT_PANEL
DBG_BOT_MODE
DBG_BOT_RESET
DBG_BOT_PARAM:LOOKAHEADWEIGHT=14,LOOKAHEADDEPTH=3,SAFENEIGHBORWEIGHT=16
```

Bot tuner panel now supports mode-preserving rollback via `RESET DEFAULT`,
which reapplies mode defaults (`safe`/`balanced`/`aggressive`) over live tweaks.
