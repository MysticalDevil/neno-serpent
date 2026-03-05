#!/usr/bin/env python3
"""Coverage gate with threshold + anti-regression checks."""

from __future__ import annotations

import argparse
import json
import pathlib
import sys


def load_json(path: pathlib.Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def extract_percent(summary: dict, key: str) -> float:
    if key in summary:
        return float(summary[key])
    nested = summary.get(key.replace("_percent", ""))
    if isinstance(nested, dict) and "percent" in nested:
        return float(nested["percent"])
    raise KeyError(f"missing coverage metric: {key}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", required=True, help="gcovr json-summary path")
    parser.add_argument(
        "--policy",
        default="docs/COVERAGE_BASELINE.json",
        help="coverage policy/baseline json",
    )
    parser.add_argument("--group", choices=["unit", "integration"], required=True)
    parser.add_argument(
        "--max-drop",
        type=float,
        default=1.0,
        help="max allowed drop vs baseline percent points",
    )
    args = parser.parse_args()

    summary = load_json(pathlib.Path(args.summary))
    policy = load_json(pathlib.Path(args.policy))
    group_cfg = policy[args.group]

    line = extract_percent(summary, "line_percent")
    branch = extract_percent(summary, "branch_percent")

    line_min = float(group_cfg["line_min"])
    branch_min = float(group_cfg["branch_min"])
    line_baseline = float(group_cfg["line_baseline"])
    branch_baseline = float(group_cfg["branch_baseline"])

    print(
        f"[coverage-gate] group={args.group} line={line:.2f}% branch={branch:.2f}% "
        f"line_min={line_min:.2f}% branch_min={branch_min:.2f}% "
        f"line_baseline={line_baseline:.2f}% branch_baseline={branch_baseline:.2f}% "
        f"max_drop={args.max_drop:.2f}%"
    )

    failures: list[str] = []
    if line < line_min:
        failures.append(f"line below min: {line:.2f}% < {line_min:.2f}%")
    if branch < branch_min:
        failures.append(f"branch below min: {branch:.2f}% < {branch_min:.2f}%")
    if line < (line_baseline - args.max_drop):
        failures.append(
            f"line regression too large: {line:.2f}% < {line_baseline - args.max_drop:.2f}%"
        )
    if branch < (branch_baseline - args.max_drop):
        failures.append(
            "branch regression too large: "
            f"{branch:.2f}% < {branch_baseline - args.max_drop:.2f}%"
        )

    if failures:
        for failure in failures:
            print(f"[coverage-gate] fail: {failure}", file=sys.stderr)
        return 1
    print("[coverage-gate] passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
