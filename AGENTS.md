# Repository-Specific Agent Notes

This file intentionally keeps only repository-specific rules and avoids repeating global AGENTS defaults.

## UI Runtime Verification Policy

- Do not use `QT_QPA_PLATFORM=offscreen` for manual UI/bot behavior verification.
- For headful UI verification, run against real Wayland display (`wl_display`).
- If sandbox blocks Wayland access, request escalated permission first, then run headful.
- Use offscreen only for test targets that are explicitly configured that way in CTest.

## Build and Runtime Entry Points

- Preset-first workflow:
  - `cmake --preset debug`
  - `cmake --build --preset debug`
  - `ctest --test-dir build/debug`
- Main app executable path:
  - `./build/<preset>/NenoSerpent`
- Preferred script entrypoints:
  - `./scripts/dev.sh`
  - `./scripts/ui.sh`
  - `./scripts/input.sh`
  - `./scripts/deploy.sh`

## Android Deployment Notes

- Use current app id: `org.devil.nenoserpent`.
- Before Android UI verification, remove legacy packages:
  - `org.devil.snakegb`
  - `org.qtproject.example.NenoSerpent`
- If Android build seems stale, clean package dir:
  - `build/android/src/android-build`

## UI Automation Constraints

- UI capture scripts must run serially; do not run capture commands in parallel.
- `scripts/ui/nav/capture.sh` owns the capture lock and should be treated as single-run global owner.
- Reusable UI script architecture details live in `docs/SCRIPT_AUTOMATION.md`.

## Current UI Injection Entry Points

- `scripts/ui.sh nav-capture dbg-choice ...` with `DBG_CHOICE_TYPES=...`
- `scripts/ui.sh nav-capture dbg-static-{boot,game,replay,choice} ...` with `DBG_STATIC_PARAMS=...`
- `scripts/ui.sh nav-capture dbg-screen-only ...`
- `scripts/ui.sh nav-capture dbg-shell-only ...`
- `scripts/ui.sh nav-debug <target>`
- Detailed format: `docs/UI_DEBUG_INJECTION.md`

## Project-Specific Code Rules

- QML layering: keep screen stack ownership in `ScreenView.qml` using named layer tokens.
- Do not place LCD-internal elements above LCD shader path just for styling.
- `OSDLayer` stays topmost above CRT/screen treatment layers.

## Related Plan Docs

- Audio system: `docs/AUDIO_SYSTEM_PLAN.md`
- Logging cleanup: `docs/LOGGING_SYSTEM_PLAN.md`
