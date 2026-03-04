# UI Debug Injection Reference

This document describes the current UI/debug injection surface used by screenshot scripts, manual debug sessions, and static preview scenes.

## Scope

The current injection interfaces cover three layers:

1. Runtime debug tokens routed by `src/qml/UiDebugController.qml`
2. Script-level target helpers in `scripts/lib/ui_nav_targets.sh`
3. Manual and automated launch flows in `scripts/ui.sh nav-capture ...` and `scripts/ui.sh nav-debug ...`

These interfaces are intended for:

- targeted screenshot capture
- manual UI inspection
- palette/readability regression coverage
- forcing specific buff/choice/static scene states without playing into them

## Entry Points

### Automated capture

Use `scripts/ui.sh nav-capture` to launch, route to a scene, and save a screenshot.

```bash
./scripts/ui.sh nav-capture menu /tmp/menu.png
DBG_CHOICE_TYPES=7,4,1 ./scripts/ui.sh nav-capture dbg-choice /tmp/choice.png
DBG_STATIC_PARAMS='BUFF=7,SCORE=55,SNAKE=6:6|7:6|8:6:H' ./scripts/ui.sh nav-capture dbg-static-game /tmp/static_game.png
```

### Manual debug

Use `scripts/ui.sh nav-debug` to launch and hold the app open on a routed target.

```bash
./scripts/ui.sh nav-debug list
./scripts/ui.sh nav-debug dbg-static-choice
DBG_STATIC_PARAMS='TITLE=POWER_PICK,CHOICES=7|4|1,INDEX=1' ./scripts/ui.sh nav-debug dbg-static-choice
```

After launch, more tokens can be injected by writing to the runtime input file:

```bash
printf 'DBG_STATIC_REPLAY:BUFF=4,SCORE=42\n' >> /tmp/nenoserpent_ui_input.txt
printf 'DBG_CHOICE:7,4,1\n' >> /tmp/nenoserpent_ui_input.txt
```

## Supported Script Targets

The shared target router in `scripts/lib/ui_nav_targets.sh` currently supports:

- `splash`
- `menu`
- `game`
- `pause`
- `pause-back`
- `pause-back-b`
- `pause-resume`
- `achievements`
- `medals`
- `replay`
- `catalog`
- `library`
- `icons`
- `icons-f6`
- `icons-right`
- `konami-on`
- `konami-off`
- `konami-on-paused`
- `konami-off-paused`
- `icons-exit-b`
- `dbg-menu`
- `dbg-play`
- `dbg-pause`
- `dbg-gameover`
- `dbg-replay`
- `dbg-replay-buff`
- `dbg-choice`
- `dbg-catalog`
- `dbg-achievements`
- `dbg-icons`
- `dbg-static-boot`
- `dbg-static-game`
- `dbg-static-replay`
- `dbg-static-choice`
- `dbg-static-off`
- `dbg-screen-only`
- `dbg-shell-only`

## Runtime Debug Tokens

The following tokens are routed by `src/qml/UiDebugController.qml`.

### Scene/state tokens

- `DBG_MENU`
- `DBG_PLAY`
- `DBG_PAUSE`
- `DBG_GAMEOVER`
- `DBG_REPLAY`
- `DBG_REPLAY_BUFF`
- `DBG_CATALOG`
- `DBG_ACHIEVEMENTS`
- `DBG_ICONS`
- `DBG_BOT_PANEL`
- `DBG_BOT_MODE`
- `DBG_BOT_RESET`
- `DBG_BOT_PARAM:KEY=VALUE[,KEY=VALUE...]`
- `DBG_STATIC_BOOT`
- `DBG_STATIC_GAME`
- `DBG_STATIC_REPLAY`
- `DBG_STATIC_CHOICE`
- `DBG_STATIC_OFF`

### Choice preview token

`DBG_CHOICE` seeds the live choice preview scene.

Formats:

```text
DBG_CHOICE
DBG_CHOICE:7,4,1
```

Rules:

- accepts up to 3 buff types
- values must be integers `1..9`
- invalid values are ignored
- no payload means "use default preview choices"

Equivalent script env var:

```bash
DBG_CHOICE_TYPES=7,4,1 ./scripts/ui.sh nav-capture dbg-choice /tmp/choice.png
```

## Static Debug Injection

Static debug scenes are driven by `DBG_STATIC_*:<params>` tokens. Script entry points expose this via `DBG_STATIC_PARAMS`.

Standalone composition debug scenes are driven by launch args instead of injected tokens:

- `dbg-screen-only` maps to `--ui-mode=screen`
- `dbg-shell-only` maps to `--ui-mode=shell`

### Supported static scenes

- `dbg-static-boot`
- `dbg-static-game`
- `dbg-static-replay`
- `dbg-static-choice`

### General syntax

```text
DBG_STATIC_<SCENE>:KEY=VALUE,KEY=VALUE,...
```

Script equivalent:

```bash
DBG_STATIC_PARAMS='KEY=VALUE,KEY=VALUE,...' ./scripts/ui.sh nav-capture dbg-static-<scene> /tmp/out.png
```

Separators and encoding rules:

- key/value pairs are comma-separated
- keys are case-insensitive
- spaces in labels can be written as `_` or `+`
- point lists use `|` or `/` between entries
- coordinates use `x:y`

## Static Boot Injection

The boot scene currently supports:

- `TITLE`
- `SUBTITLE`
- `LOAD` or `LOADLABEL`
- `PROGRESS` or `LOADPROGRESS`

Example:

```bash
DBG_STATIC_PARAMS='TITLE=S_N_A_K_E+,SUBTITLE=PORTABLE_ARCADE_SURVIVAL,LOAD=BOOTING_88%,PROGRESS=0.88' \
  ./scripts/ui.sh nav-capture dbg-static-boot /tmp/static_boot.png
```

Behavior:

- `TITLE` drives the large boot title
- `SUBTITLE` drives the small subtitle under the title
- `LOAD` / `LOADLABEL` drives the loading label text
- `PROGRESS` / `LOADPROGRESS` accepts `0..1` or `0..100`

## Static Board Injection

The `game` and `replay` static scenes support these board-facing keys:

- `BUFF`
- `SCORE`
- `HI`
- `REMAIN`
- `TOTAL`
- `SNAKE`
- `FOOD`
- `OBSTACLES`
- `POWERUPS`

Example:

```bash
DBG_STATIC_PARAMS='BUFF=7,SCORE=55,HI=120,REMAIN=96,TOTAL=180,SNAKE=6:6|7:6|8:6:H,FOOD=13:10|15:12,OBSTACLES=16:8|17:8,POWERUPS=18:10:7' \
  ./scripts/ui.sh nav-capture dbg-static-game /tmp/static_game.png
```

### Board parameter formats

`BUFF`

- integer `1..9`

`SCORE`, `HI`, `REMAIN`

- non-negative integers

`TOTAL`

- integer `>= 1`

`SNAKE`

- list of points in `x:y` or `x:y:H`
- `H`, `h`, or `1` marks a segment as the head

Example:

```text
SNAKE=6:6|7:6|8:6:H
```

`FOOD`

- list of points in `x:y`

Example:

```text
FOOD=13:10|15:12
```

`OBSTACLES`

- list of points in `x:y`

`POWERUPS`

- list of `x:y:type`
- `type` must be `1..9`

Example:

```text
POWERUPS=18:10:7|11:8:4
```

## Static Choice Injection

The static choice scene supports:

- `TITLE`
- `SUBTITLE`
- `FOOTER` or `FOOTERHINT`
- `CHOICES` or `TYPES`
- `INDEX`

Example:

```bash
DBG_STATIC_PARAMS='TITLE=POWER_PICK,SUBTITLE=CHOOSE_YOUR_LOADOUT,FOOTER=START_CONFIRM___SELECT_BACK,CHOICES=7|4|1,INDEX=1' \
  ./scripts/ui.sh nav-capture dbg-static-choice /tmp/static_choice.png
```

Behavior:

- `TITLE` drives the choice header title
- `SUBTITLE` drives the choice header subtitle
- `FOOTER` / `FOOTERHINT` drives the footer hint line
- `CHOICES` / `TYPES` sets up to 3 buff types
- `INDEX` selects the highlighted choice

Additional shorthand:

- bare integers without keys are treated as choice types in the `choice` scene

Example:

```text
DBG_STATIC_CHOICE:7,4,1,INDEX=1
```

## Current Coverage Summary

### Fully injectable now

- live choice preview buff types via `DBG_CHOICE`
- static boot title/subtitle/loading label/loading progress
- static game/replay score, high score, buff state
- static game/replay board sample cells for snake, food, obstacles, powerups
- static choice title/subtitle/footer/types/selected index

### Intentionally still derived

Some values are still derived from buff type rather than fully freeform objects:

- choice card titles
- choice card descriptions
- choice card rarity labels
- buff glyphs and rarity semantics

In other words, `CHOICES=7|4|1` injects buff types, not arbitrary card payloads like:

```json
[
  { "title": "...", "desc": "...", "rarity": "...", "glyph": "..." }
]
```

That is the current boundary.

## Recommended Validation Flow

When changing these interfaces:

1. Build first:

```bash
cmake --build build/dev --parallel
```

2. If a shell script changed, lint it:

```bash
shellcheck -P SCRIPTDIR -x scripts/lib/ui_nav_targets.sh
```

3. Run at least two screenshot rounds:

- a default or low-filter round, usually `PALETTE_STEPS=0`
- a high-filter round, usually `PALETTE_STEPS=4`

Suggested coverage:

```bash
DBG_STATIC_PARAMS='TITLE=S_N_A_K_E+,SUBTITLE=PORTABLE_ARCADE_SURVIVAL,LOAD=BOOTING_88%,PROGRESS=0.88' \
  ./scripts/ui.sh nav-capture dbg-static-boot /tmp/static_boot_p0.png

PALETTE_STEPS=4 DBG_STATIC_PARAMS='TITLE=S_N_A_K_E+,SUBTITLE=PORTABLE_ARCADE_SURVIVAL,LOAD=BOOTING_88%,PROGRESS=0.88' \
  ./scripts/ui.sh nav-capture dbg-static-boot /tmp/static_boot_p4.png

DBG_STATIC_PARAMS='BUFF=7,SCORE=55,HI=120,SNAKE=6:6|7:6|8:6:H,FOOD=13:10|15:12,OBSTACLES=16:8|17:8,POWERUPS=18:10:7' \
  ./scripts/ui.sh nav-capture dbg-static-game /tmp/static_game_p0.png

PALETTE_STEPS=4 DBG_STATIC_PARAMS='TITLE=POWER_PICK,SUBTITLE=CHOOSE_YOUR_LOADOUT,FOOTER=START_CONFIRM___SELECT_BACK,CHOICES=7|4|1,INDEX=1' \
  ./scripts/ui.sh nav-capture dbg-static-choice /tmp/static_choice_p4.png
```

## Files To Update When Expanding Injection

When new injectable UI surfaces are added, keep these in sync:

- `src/qml/UiDebugController.qml`
- `src/qml/StaticDebugLayer.qml`
- `src/qml/LevelUpModal.qml` if static choice header/footer bindings change
- `scripts/lib/ui_nav_targets.sh`
- `AGENTS.md`
- this document
