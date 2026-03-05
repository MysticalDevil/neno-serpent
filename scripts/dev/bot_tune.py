#!/usr/bin/env python3
"""Bot strategy tuner for NenoSerpent.

Purpose:
- Run offline random-search tuning against fixed benchmark scenarios.
- Emit a reusable strategy override JSON that can be loaded via
  NENOSERPENT_BOT_STRATEGY_FILE.

Inputs:
- bot-benchmark binary path
- profile/mode
- fixed levels and seeds
- iteration budget

Output:
- JSON file containing tuned profile overrides
"""

from __future__ import annotations

import argparse
import json
import os
import random
import re
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path

SCORE_AVG_RE = re.compile(r"score\.avg=([0-9]+(?:\.[0-9]+)?)")
SCORE_P95_RE = re.compile(r"score\.p95=([0-9]+)")
TIMEOUT_RE = re.compile(r"outcomes\.timeout=([0-9]+)")


@dataclass(frozen=True)
class ParamRange:
    lo: int
    hi: int
    step: int


PARAM_RANGES: dict[str, ParamRange] = {
    "openSpaceWeight": ParamRange(0, 60, 1),
    "safeNeighborWeight": ParamRange(0, 80, 1),
    "targetDistanceWeight": ParamRange(0, 80, 1),
    "straightBonus": ParamRange(0, 40, 1),
    "foodConsumeBonus": ParamRange(0, 80, 1),
    "trapPenalty": ParamRange(0, 120, 1),
    "lookaheadDepth": ParamRange(0, 4, 1),
    "lookaheadWeight": ParamRange(0, 60, 1),
    "powerTargetPriorityThreshold": ParamRange(0, 100, 1),
    "powerTargetDistanceSlack": ParamRange(0, 20, 1),
    "choiceCooldownTicks": ParamRange(0, 60, 1),
    "stateActionCooldownTicks": ParamRange(0, 60, 1),
}


def clamp_param(name: str, value: int) -> int:
    r = PARAM_RANGES[name]
    clamped = max(r.lo, min(r.hi, value))
    if r.step <= 1:
        return clamped
    bucket = round((clamped - r.lo) / r.step)
    return r.lo + bucket * r.step


def parse_output(text: str) -> tuple[float, int, int]:
    avg_match = SCORE_AVG_RE.search(text)
    p95_match = SCORE_P95_RE.search(text)
    timeout_match = TIMEOUT_RE.search(text)
    if avg_match is None or p95_match is None or timeout_match is None:
        raise RuntimeError(f"failed to parse bot-benchmark output:\n{text}")
    return float(avg_match.group(1)), int(p95_match.group(1)), int(timeout_match.group(1))


def load_base_profiles(strategy_file: Path) -> dict:
    data = json.loads(strategy_file.read_text(encoding="utf-8"))
    profiles = data.get("profiles")
    if not isinstance(profiles, dict):
        raise RuntimeError(f"invalid strategy file, missing object 'profiles': {strategy_file}")
    if "default" not in profiles or not isinstance(profiles["default"], dict):
        raise RuntimeError(f"invalid strategy file, missing object profile 'default': {strategy_file}")
    return data


def make_candidate_from_profile(base: dict, profile: str) -> dict[str, int]:
    profiles = base["profiles"]
    merged: dict[str, int] = {}
    for key, value in profiles.get("default", {}).items():
        if key in PARAM_RANGES and isinstance(value, int):
            merged[key] = value
    for key, value in profiles.get(profile, {}).items():
        if key in PARAM_RANGES and isinstance(value, int):
            merged[key] = value
    for name, range_info in PARAM_RANGES.items():
        if name not in merged:
            merged[name] = range_info.lo
        merged[name] = clamp_param(name, merged[name])
    return merged


def random_candidate() -> dict[str, int]:
    return {
        name: random.randrange(r.lo, r.hi + 1, r.step)
        for name, r in PARAM_RANGES.items()
    }


def mutate_candidate(source: dict[str, int], edits: int = 3) -> dict[str, int]:
    out = dict(source)
    keys = list(PARAM_RANGES.keys())
    random.shuffle(keys)
    for name in keys[:edits]:
        r = PARAM_RANGES[name]
        delta_step = random.choice([-4, -3, -2, -1, 1, 2, 3, 4])
        out[name] = clamp_param(name, out[name] + delta_step * r.step)
    return out


def write_strategy_override(base_profiles: dict, profile: str, candidate: dict[str, int], path: Path) -> None:
    merged = json.loads(json.dumps(base_profiles))
    profiles = merged.setdefault("profiles", {})
    profiles[profile] = {**profiles.get(profile, {}), **candidate}
    path.write_text(json.dumps(merged, indent=2) + "\n", encoding="utf-8")


def run_scenarios(
    benchmark_bin: Path,
    strategy_override: Path,
    profile: str,
    mode: str,
    levels: list[int],
    seeds: list[int],
    games: int,
    max_ticks: int,
) -> tuple[float, float, int]:
    avg_values: list[float] = []
    p95_values: list[int] = []
    timeout_total = 0

    for level in levels:
        for seed in seeds:
            cmd = [
                str(benchmark_bin),
                "--games",
                str(games),
                "--max-ticks",
                str(max_ticks),
                "--seed",
                str(seed),
                "--level",
                str(level),
                "--profile",
                profile,
                "--mode",
                mode,
                "--backend",
                "rule",
                "--strategy-file",
                str(strategy_override),
            ]
            out = subprocess.check_output(cmd, text=True)
            avg, p95, timeout = parse_output(out)
            avg_values.append(avg)
            p95_values.append(p95)
            timeout_total += timeout

    avg_mean = sum(avg_values) / len(avg_values)
    p95_mean = sum(p95_values) / len(p95_values)
    return avg_mean, p95_mean, timeout_total


def objective(avg_mean: float, p95_mean: float, timeout_total: int) -> float:
    return avg_mean + 0.35 * p95_mean - 0.4 * timeout_total


def parse_int_list(raw: str) -> list[int]:
    values = []
    for token in raw.split(","):
        token = token.strip()
        if not token:
            continue
        values.append(int(token))
    if not values:
        raise ValueError("empty integer list")
    return values


def main() -> int:
    repo_root = Path(__file__).resolve().parents[2]
    tmp_root = os.environ.get("NENOSERPENT_TMP_DIR") or os.environ.get(
        "NENOSERPENT_CACHE_DIR"
    ) or str(repo_root / "cache" / "dev")
    parser = argparse.ArgumentParser(description="Tune bot strategy parameters via offline random search.")
    parser.add_argument("--benchmark-bin", required=True, help="Path to bot-benchmark binary")
    parser.add_argument(
        "--strategy-file",
        default="src/adapter/bot/strategy_profiles.json",
        help="Base strategy profile JSON",
    )
    parser.add_argument("--profile", default="debug", help="Profile key to tune (debug/dev/release)")
    parser.add_argument("--mode", default="balanced", choices=["safe", "balanced", "aggressive"])
    parser.add_argument("--levels", default="0,1", help="Comma-separated level list")
    parser.add_argument("--seeds", default="1337,1438,1539,1640", help="Comma-separated seed list")
    parser.add_argument("--games", type=int, default=20)
    parser.add_argument("--max-ticks", type=int, default=2400)
    parser.add_argument("--iterations", type=int, default=60)
    parser.add_argument("--explore-ratio", type=float, default=0.3)
    parser.add_argument("--output", required=True, help="Output strategy JSON path")
    parser.add_argument("--report", default=f"{tmp_root}/nenoserpent_bot_tune_report.json")
    parser.add_argument("--seed", type=int, default=20260304, help="Random seed for reproducible tuning")
    args = parser.parse_args()

    random.seed(args.seed)
    benchmark_bin = Path(args.benchmark_bin).resolve()
    if not benchmark_bin.is_file():
        raise RuntimeError(f"benchmark binary not found: {benchmark_bin}")

    base_profiles = load_base_profiles(Path(args.strategy_file).resolve())
    levels = parse_int_list(args.levels)
    seeds = parse_int_list(args.seeds)

    current = make_candidate_from_profile(base_profiles, args.profile)
    best = dict(current)
    best_score = float("-inf")
    history: list[dict] = []

    with tempfile.TemporaryDirectory(prefix="nenoserpent-bot-tune-") as tmp_dir:
        tmp_path = Path(tmp_dir) / "strategy_override.json"
        for idx in range(args.iterations):
            if idx == 0:
                candidate = dict(current)
            elif random.random() < args.explore_ratio:
                candidate = random_candidate()
            else:
                edits = random.choice([2, 3, 4, 5])
                candidate = mutate_candidate(best, edits=edits)

            write_strategy_override(base_profiles, args.profile, candidate, tmp_path)
            avg_mean, p95_mean, timeout_total = run_scenarios(
                benchmark_bin=benchmark_bin,
                strategy_override=tmp_path,
                profile=args.profile,
                mode=args.mode,
                levels=levels,
                seeds=seeds,
                games=max(1, args.games),
                max_ticks=max(200, args.max_ticks),
            )
            score = objective(avg_mean, p95_mean, timeout_total)
            history.append(
                {
                    "iteration": idx + 1,
                    "score": score,
                    "avg_mean": avg_mean,
                    "p95_mean": p95_mean,
                    "timeout_total": timeout_total,
                    "candidate": candidate,
                }
            )

            if score > best_score:
                best_score = score
                best = dict(candidate)
                print(
                    f"[bot-tune] iter={idx + 1} new-best "
                    f"score={score:.3f} avg={avg_mean:.3f} p95={p95_mean:.3f} timeout={timeout_total}"
                )

    output_path = Path(args.output).resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    write_strategy_override(base_profiles, args.profile, best, output_path)

    report = {
        "profile": args.profile,
        "mode": args.mode,
        "levels": levels,
        "seeds": seeds,
        "games": args.games,
        "max_ticks": args.max_ticks,
        "iterations": args.iterations,
        "best_score": best_score,
        "best_candidate": best,
        "output": str(output_path),
        "history_tail": history[-10:],
    }
    report_path = Path(args.report).resolve()
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"[bot-tune] best output: {output_path}")
    print(f"[bot-tune] report: {report_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
