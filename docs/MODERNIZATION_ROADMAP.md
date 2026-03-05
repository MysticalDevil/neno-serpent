# NenoSerpent Modernization Roadmap

## Scope

This roadmap covers modernization targets outside language migration.  
Focus: engineering workflow, quality gates, observability, UI regression, release pipeline, and
runtime robustness.

## 3-Week Delivery Plan

### Week 1: Environment and Quality Gate Baseline

#### Goals
- Standardize local/CI toolchain versions and startup commands.
- Split quality checks into fast gate (PR) and full gate (nightly/manual).
- Reduce "works on my machine" drift for Qt/CMake/Android build paths.

#### Deliverables
- Add one reproducible developer environment entrypoint:
  - `devcontainer` or
  - `mise` task + toolchain pinning document.
- Define unified quality tasks:
  - `just fmt`
  - `just lint`
  - `just test`
  - `just check` (minimum full gate for touched scope)
- Update CI strategy:
  - PR: fast gate only
  - nightly: full gate (UI captures + benchmark/e2e suite)

#### Acceptance Criteria
- Fresh machine bootstrap guide runs end-to-end without ad-hoc steps.
- PR pipeline runtime decreases while preserving core regression detection.
- Team can run the same gate locally and in CI with identical command names.

### Week 2: Observability and UI Regression Productization

#### Goals
- Make runtime diagnostics queryable and mode-aware (`release/dev/debug`).
- Convert screenshot checks from ad-hoc inspection to baseline-driven comparison.
- Improve issue triage speed on Android and desktop.

#### Deliverables
- Logging upgrades:
  - keep category separation (`state/audio/input/inject/level/replay`)
  - add correlation field (session/run id) for grouped traces
  - add JSON export mode for machine parsing (dev/debug only)
- UI regression upgrades:
  - baseline image directory with update workflow
  - diff output (heatmap + summary)
  - threshold policy per capture target

#### Acceptance Criteria
- Given one bug report, logs can be filtered by category and run id in under 2 minutes.
- UI regression command returns pass/fail + artifact bundle instead of only raw PNGs.
- Android and desktop use the same regression command surface.

### Week 3: Release Hardening and Data/Config Governance

#### Goals
- Harden release pipeline outputs and traceability.
- Prevent runtime failures caused by malformed data/config resources.
- Add stable performance/latency trend tracking.

#### Deliverables
- Release hardening:
  - signed artifact checks integrated into release jobs
  - checksums + SBOM generation for shipped binaries
  - release note template with required verification fields
- Data/config governance:
  - schema validation for themes/levels/audio/bot resource payloads in CI
  - startup-time validation summary (dev/debug mode)
- Performance baseline:
  - fixed scenario benchmark set
  - metrics: FPS, frame jitter, input latency, Android stutter markers

#### Acceptance Criteria
- Every release artifact has signature/checksum/SBOM attached.
- Invalid resource files fail in CI before packaging.
- Performance regressions are detectable with a historical comparison report.

## Prioritization Rules

- Prefer changes that reduce debugging time for current active platforms (desktop + Android).
- Prefer one-command workflows (`just ...`) over multi-step tribal knowledge.
- Avoid introducing new tools unless they replace duplicated manual work.

## Non-Goals

- No gameplay feature redesign in this roadmap.
- No mandatory runtime dependency expansion unless directly required by the goals above.

