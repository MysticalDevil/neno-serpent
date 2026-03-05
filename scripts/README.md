# Scripts Directory Guide

This directory follows a layered layout:

- `scripts/dev.sh`: development and bot workflows entrypoint.
- `scripts/ui.sh`: UI capture/debug automation entrypoint.
- `scripts/input.sh`: input injection/semantics entrypoint.
- `scripts/deploy.sh`: deployment entrypoint.
- `scripts/dev/`: implementation scripts for dev/bot tasks.
- `scripts/ui/`: implementation scripts for UI automation.
- `scripts/input/`: implementation scripts for input validation and probes.
- `scripts/deploy/`: implementation scripts for deploy targets.
- `scripts/ci/`: CI/regression gates and suites.
- `scripts/lib/`: shared shell helpers.

## Conventions

- Keep entrypoints (`*.sh` in `scripts/`) thin: parse args, dispatch, `exec`.
- Put heavy data processing in Python modules under the same domain directory.
- Keep shell scripts focused on orchestration and tool invocation.
- Prefer repository cache paths via `NENOSERPENT_CACHE_DIR` / `NENOSERPENT_TMP_DIR`.
- Run `shellcheck` on touched shell scripts and `uv run python -m py_compile` on touched Python scripts.

## Public Entry Points

- `./scripts/dev.sh`
- `./scripts/ui.sh`
- `./scripts/input.sh`
- `./scripts/deploy.sh`

Call `--help` on each command for subcommand usage.
