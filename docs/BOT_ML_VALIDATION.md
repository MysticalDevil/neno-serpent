# Bot ML Validation

This document defines the reproducible local validation flow for `rule` vs `ml`.
It also defines the reproducible publish-loop validation for `ml-online`.

## Goal

Run a full chain with fixed seed and fixed suite:

1. generate dataset
2. train model
3. export runtime JSON
4. run leaderboard compare (`rule` vs `ml`)
5. enforce **ml no-regression** gate

## One-Command Flow

```bash
./scripts/dev.sh bot-ml-gate --workspace cache/dev/nenoserpent_bot_ml_gate
```

Quick smoke gate (repo tiny model):

```bash
./scripts/dev.sh bot-ml-smoke build/dev
```

`ml-online` loop gate:

```bash
./scripts/dev.sh bot-ml-online-gate --workspace cache/dev/nenoserpent_bot_ml_online_gate --rounds 3
```

Outputs:

- `cache/dev/nenoserpent_bot_ml_gate/dataset.csv`
- `cache/dev/nenoserpent_bot_ml_gate/policy.pt`
- `cache/dev/nenoserpent_bot_ml_gate/policy.runtime.json`
- `cache/dev/nenoserpent_bot_ml_gate/eval.report.json`
- `cache/dev/nenoserpent_bot_ml_gate/leaderboard.compare.tsv`
- `cache/dev/nenoserpent_bot_ml_online_gate/online.summary.env`

## Gate Rule

The compare gate is enforced by:

```bash
BOT_LEADERBOARD_REQUIRE_NO_REGRESSION=1
```

Pass condition per matched case (`id + mode + level + seed`):

- `ml_avg >= rule_avg`
- `ml_p95 >= rule_p95`

If any case violates either constraint, the script exits with non-zero status.

## Notes

- The leaderboard suite already contains both `rule` and `ml` rows.
- `ml` rows require `BOT_ML_MODEL` (runtime JSON model path).
- Without `BOT_ML_MODEL`, `ml` rows are skipped by design.

## CI Optional Gate

GitHub Actions `cmake.yml` supports optional ML no-regression gate.

If repository secret `BOT_ML_MODEL_B64` is set (base64 of runtime JSON model):

1. decode model to `cache/ci/nenoserpent_bot_policy_runtime.json`
2. run leaderboard compare with `BOT_LEADERBOARD_REQUIRE_NO_REGRESSION=1`
3. fail CI if any `ml` case regresses below `rule`

## ML-Online Gate Rule

Validation entrypoint:

```bash
./scripts/dev.sh bot-ml-online-gate --workspace cache/dev/nenoserpent_bot_ml_online_gate --rounds 3
```

Pass conditions:

- `online.summary.env` exists
- `rounds >= requested rounds`
- `published >= 1`
- `runtime_json` exists

This gate validates the online training/publish control loop itself.  
Score-strength comparison against `rule` remains covered by the existing `bot-ml-gate`.

## CI Always-On Smoke

`cmake.yml` also runs an always-on smoke gate using repository tiny model:

- script: `scripts/ci/bot_ml_smoke_gate.sh`
- model: `scripts/ci/assets/bot_ml_smoke.runtime.json`
- policy: `ml_avg >= rule_avg` and `ml_p95 >= rule_p95` on a fixed quick scenario
