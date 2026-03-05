# Bot Training (PyTorch)

This document defines the current imitation-learning baseline for NenoSerpent bot.

## Scope

- Source policy: existing rule bot (`safe|balanced|aggressive`).
- Training target: action class (`up|right|down|left`).
- Model: small MLP classifier.
- Runtime integration: wired via `ml`/`ml-online` backend
  (`NENOSERPENT_BOT_ML_MODEL=<runtime-json>`).

Prerequisite:

- Python environment with `torch` installed.

## 1. Generate Dataset

Use fixed leaderboard suite as data source:

```bash
./scripts/dev.sh bot-dataset --output cache/dev/nenoserpent_bot_dataset.csv
```

Optional knobs:

- `--profile debug|dev|release`
- `--suite /abs/path/to/suite.tsv`
- `--max-samples-per-case N`

Dataset schema is emitted by `bot-benchmark --dump-dataset` and includes:

- game snapshot features (`head`, `dir`, `food_delta`, powerup flags, danger around head, etc.)
- supervised label `action` (`0=up,1=right,2=down,3=left`)

## 2. Train Imitation Model

```bash
./scripts/dev.sh bot-train \
  --dataset cache/dev/nenoserpent_bot_dataset.csv \
  --model cache/dev/nenoserpent_bot_policy.pt \
  --metadata cache/dev/nenoserpent_bot_policy_meta.json \
  --runtime-json cache/dev/nenoserpent_bot_policy_runtime.json
```

Important args:

- `--epochs` (default `30`)
- `--batch-size` (default `256`)
- `--lr` (default `1e-3`)
- `--train-ratio` (default `0.9`)
- `--runtime-json` (default `cache/dev/nenoserpent_bot_policy_runtime.json`)
- `--hard-sample-scale` (default `1.0`)

Training split behavior:

- if dataset has `seed` column, split is grouped by seed (reduce train/val leakage)
- otherwise fallback to row-level random split

Training sampling behavior:

- train epoch uses weighted resampling toward difficult states (high local danger / long body)
- validation remains unweighted

## 3. Evaluate

```bash
./scripts/dev.sh bot-eval \
  --dataset cache/dev/nenoserpent_bot_dataset.csv \
  --model cache/dev/nenoserpent_bot_policy.pt \
  --report cache/dev/nenoserpent_bot_eval_report.json
```

Current eval metric:

- action classification accuracy on provided dataset

Runtime backend can load the exported JSON directly:

```bash
NENOSERPENT_BOT_ML_MODEL=cache/dev/nenoserpent_bot_policy_runtime.json ./build/dev/NenoSerpent
```

For online evolution (`ml-online`), run a continuous training loop in parallel:

```bash
./scripts/dev.sh bot-online-train --workspace cache/dev/nenoserpent_bot_online
./scripts/dev.sh bot-run --backend ml-online \
  --ml-model cache/dev/nenoserpent_bot_online/nenoserpent_bot_policy_runtime.json --headful
```

Online publish gate (enabled by default in `bot-online-train`):

- Each round benchmarks `current` vs `candidate` runtime JSON with identical seed/settings.
- Candidate is published only when both `score.avg` and `score.p95` are not lower than current.
- Optional tolerance: `--gate-eps <float>` (or `BOT_ONLINE_GATE_NO_REGRESSION_EPS`).
- Cache growth guard in loop:
  - `--cache-max-mb` high-water mark
  - `--cache-target-mb` prune target after high-water is exceeded

## 4. Offline Tuning + Training Loop

Recommended loop:

1. Run `bot-leaderboard` baseline
2. Run `bot-tune` to get stronger rule policy JSON
3. Re-generate dataset with tuned policy as teacher (set `NENOSERPENT_BOT_STRATEGY_FILE`)
4. Re-train and re-evaluate
5. Re-check leaderboard to ensure gameplay score did not regress

## 5. Runtime Regression Check

After training a new model, run a compare gate before adoption:

```bash
BOT_ML_MODEL=cache/dev/nenoserpent_bot_policy_runtime.json \
BOT_LEADERBOARD_REQUIRE_NO_REGRESSION=1 \
./scripts/dev.sh bot-leaderboard build/dev
```
