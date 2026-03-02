# Bot Rules and Tuning

NenoSerpent uses a rule-based bot with a tunable strategy config.

## Runtime Architecture

- `adapter/bot/controller.*`: movement and choice scoring.
- `adapter/bot/runtime.*`: state-aware bot flow (`Playing`, `ChoiceSelection`, `GameOver`, etc.).
- `adapter/bot/config.*`: profile loading, default values, build-profile selection.

`EngineAdapter` only injects the current game snapshot and executes bot outputs.

Mode behavior (`F8`):

- `off`: bot disabled
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
./scripts/dev.sh bot-benchmark --games 300 --max-ticks 5000 --profile dev --mode balanced
```

The benchmark reports max/avg/median/p95 score and game-over/timeout outcomes.

## Live Debug Tuning

Debug tokens accepted by runtime injection:

- `DBG_BOT_PANEL` (toggle panel visibility)
- `DBG_BOT_MODE` (cycle mode)
- `DBG_BOT_PARAM:KEY=VALUE[,KEY=VALUE...]`

Examples:

```text
DBG_BOT_PANEL
DBG_BOT_MODE
DBG_BOT_PARAM:LOOKAHEADWEIGHT=14,LOOKAHEADDEPTH=3,SAFENEIGHBORWEIGHT=16
```
