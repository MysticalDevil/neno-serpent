# Test Coverage Policy

## Scope

Coverage gates are split into two groups:

- `unit`: fast component/unit tests
- `integration`: high-level stateful/integration tests

Both groups are enforced in CI.

## Gate Rules

Minimum thresholds:

- unit: line `>= 70%`, branch `>= 55%`
- integration: line `>= 40%`, branch `>= 25%`

Anti-regression:

- max drop from baseline is `1.0%` for line and branch coverage
- baseline values are tracked in `docs/COVERAGE_BASELINE.json`

## CI Mapping

Unit subset:

- all tests except:
  - `EngineAdapterTest`
  - `SessionRunnerTest`
  - `AdapterUiCommandTest`

Integration subset:

- `EngineAdapterTest`
- `SessionRunnerTest`
- `AdapterUiCommandTest`

## Local Reproduction

```bash
cmake --preset coverage --fresh
cmake --build --preset coverage

find build/coverage -name '*.gcda' -delete
QT_QPA_PLATFORM=offscreen ctest --preset coverage -E "EngineAdapterTest|SessionRunnerTest|AdapterUiCommandTest" --output-on-failure
./scripts/ci/coverage_collect.sh build/coverage /tmp/coverage-unit-summary.json
python3 ./scripts/ci/coverage_gate.py --summary /tmp/coverage-unit-summary.json --group unit

find build/coverage -name '*.gcda' -delete
QT_QPA_PLATFORM=offscreen ctest --preset coverage -R "EngineAdapterTest|SessionRunnerTest|AdapterUiCommandTest" --output-on-failure
./scripts/ci/coverage_collect.sh build/coverage /tmp/coverage-integration-summary.json
python3 ./scripts/ci/coverage_gate.py --summary /tmp/coverage-integration-summary.json --group integration
```

