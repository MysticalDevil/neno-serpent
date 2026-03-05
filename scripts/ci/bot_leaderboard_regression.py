#!/usr/bin/env python3
"""Leaderboard regression runner.

Shell wrapper passes argv/env; metrics parsing and TSV aggregation stay in Python.
"""

from __future__ import annotations

import argparse
import csv
import os
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

AVG_RE = re.compile(r"score\.avg=([0-9]+(?:\.[0-9]+)?)")
P95_RE = re.compile(r"score\.p95=([0-9]+)")
TIMEOUT_RE = re.compile(r"outcomes\.timeout=([0-9]+)")
CHOICE_MATCH_RE = re.compile(r"choice\.match_rate=([0-9]+(?:\.[0-9]+)?)")
CHOICE_GAP_RE = re.compile(r"choice\.avg_gap=(-?[0-9]+(?:\.[0-9]+)?)")


@dataclass(frozen=True)
class Row:
    case_id: str
    backend: str
    mode: str
    level: str
    seed: str
    games: int
    max_ticks: int
    avg: float
    p95: int
    timeout: int
    choice_match_rate: float
    choice_avg_gap: float

    @property
    def rank_score(self) -> float:
        return self.avg + (0.35 * self.p95) - (0.4 * self.timeout)


def run_cmd(cmd: list[str]) -> str:
    result = subprocess.run(cmd, text=True, capture_output=True, check=False)
    if result.returncode != 0:
        raise RuntimeError(
            f"command failed ({result.returncode}): {' '.join(cmd)}\n"
            f"stdout:\n{result.stdout}\n"
            f"stderr:\n{result.stderr}"
        )
    return result.stdout + result.stderr


def parse_metric(pattern: re.Pattern[str], output: str, label: str) -> str:
    match = pattern.search(output)
    if match is None:
        raise RuntimeError(f"parse failed for {label}")
    return match.group(1)


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root-dir", required=True)
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--suite-file", required=True)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--result-file", required=True)
    parser.add_argument("--summary-file", required=True)
    parser.add_argument("--compare-file", required=True)
    parser.add_argument("--ml-model", default="")
    parser.add_argument("--require-no-regression", type=int, default=0)
    return parser.parse_args(argv)


def load_suite_rows(path: Path) -> list[list[str]]:
    out: list[list[str]] = []
    with path.open("r", encoding="utf-8") as f:
        for line in f:
            text = line.strip()
            if not text or text.startswith("#"):
                continue
            out.append(text.split("\t"))
    return out


def format_f3(value: float) -> str:
    return f"{value:.3f}"


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    root = Path(args.root_dir).resolve()
    bin_path = (root / args.build_dir / "bot-benchmark").resolve()
    suite_file = Path(args.suite_file).resolve()
    result_file = Path(args.result_file).resolve()
    summary_file = Path(args.summary_file).resolve()
    compare_file = Path(args.compare_file).resolve()

    if not bin_path.is_file():
        print(f"[bot-leaderboard] missing benchmark binary: {bin_path}", file=sys.stderr)
        return 1
    if not suite_file.is_file():
        print(f"[bot-leaderboard] missing suite file: {suite_file}", file=sys.stderr)
        return 1

    rows: list[Row] = []
    for cols in load_suite_rows(suite_file):
        if len(cols) != 7:
            print(f"[bot-leaderboard] invalid suite row: {' '.join(cols)}", file=sys.stderr)
            return 1
        case_id, backend, mode, level, seed, games_raw, max_ticks_raw = cols
        games = int(games_raw)
        max_ticks = int(max_ticks_raw)

        if backend == "ml" and not args.ml_model:
            print(f"[bot-leaderboard] skip ml row without BOT_ML_MODEL: id={case_id}", file=sys.stderr)
            continue

        cmd = [
            str(bin_path),
            "--games",
            str(games),
            "--max-ticks",
            str(max_ticks),
            "--seed",
            seed,
            "--level",
            level,
            "--profile",
            args.profile,
            "--mode",
            mode,
            "--backend",
            backend,
        ]
        if backend == "ml":
            cmd.extend(["--ml-model", args.ml_model])
        output = run_cmd(cmd)

        try:
            avg = float(parse_metric(AVG_RE, output, "score.avg"))
            p95 = int(parse_metric(P95_RE, output, "score.p95"))
            timeout = int(parse_metric(TIMEOUT_RE, output, "outcomes.timeout"))
            choice_match_rate = float(parse_metric(CHOICE_MATCH_RE, output, "choice.match_rate"))
            choice_avg_gap = float(parse_metric(CHOICE_GAP_RE, output, "choice.avg_gap"))
        except RuntimeError:
            print(f"[bot-leaderboard] parse failed id={case_id}", file=sys.stderr)
            return 1

        rows.append(
            Row(
                case_id=case_id,
                backend=backend,
                mode=mode,
                level=level,
                seed=seed,
                games=games,
                max_ticks=max_ticks,
                avg=avg,
                p95=p95,
                timeout=timeout,
                choice_match_rate=choice_match_rate,
                choice_avg_gap=choice_avg_gap,
            )
        )

    result_file.parent.mkdir(parents=True, exist_ok=True)
    with result_file.open("w", encoding="utf-8", newline="") as f:
        w = csv.writer(f, delimiter="\t")
        w.writerow(
            [
                "id",
                "backend",
                "mode",
                "level",
                "seed",
                "games",
                "max_ticks",
                "avg",
                "p95",
                "timeout",
                "choice_match_rate",
                "choice_avg_gap",
                "rank_score",
            ]
        )
        for row in rows:
            w.writerow(
                [
                    row.case_id,
                    row.backend,
                    row.mode,
                    row.level,
                    row.seed,
                    row.games,
                    row.max_ticks,
                    row.avg,
                    row.p95,
                    row.timeout,
                    row.choice_match_rate,
                    row.choice_avg_gap,
                    format_f3(row.rank_score),
                ]
            )

    summary_file.parent.mkdir(parents=True, exist_ok=True)
    with summary_file.open("w", encoding="utf-8", newline="") as f:
        w = csv.writer(f, delimiter="\t")
        w.writerow(
            [
                "backend",
                "mode",
                "cases",
                "avg_mean",
                "p95_mean",
                "timeout_sum",
                "choice_match_rate_mean",
                "choice_avg_gap_mean",
                "rank_score_mean",
            ]
        )
        by_key: dict[tuple[str, str], list[Row]] = {}
        for row in rows:
            by_key.setdefault((row.backend, row.mode), []).append(row)
        for (backend, mode) in sorted(by_key):
            group = by_key[(backend, mode)]
            cases = len(group)
            avg_mean = sum(r.avg for r in group) / cases
            p95_mean = sum(r.p95 for r in group) / cases
            timeout_sum = sum(r.timeout for r in group)
            choice_match_mean = sum(r.choice_match_rate for r in group) / cases
            choice_gap_mean = sum(r.choice_avg_gap for r in group) / cases
            rank_mean = sum(r.rank_score for r in group) / cases
            w.writerow(
                [
                    backend,
                    mode,
                    cases,
                    format_f3(avg_mean),
                    format_f3(p95_mean),
                    timeout_sum,
                    format_f3(choice_match_mean),
                    format_f3(choice_gap_mean),
                    format_f3(rank_mean),
                ]
            )

    compare_rows: list[list[str]] = []
    rule_map: dict[tuple[str, str, str, str], Row] = {}
    for row in rows:
        key = (row.case_id, row.mode, row.level, row.seed)
        if row.backend == "rule":
            rule_map[key] = row
    for row in rows:
        if row.backend != "ml":
            continue
        key = (row.case_id, row.mode, row.level, row.seed)
        rule = rule_map.get(key)
        if rule is None:
            continue
        delta_avg = row.avg - rule.avg
        delta_p95 = row.p95 - rule.p95
        delta_choice_match = row.choice_match_rate - rule.choice_match_rate
        delta_choice_gap = row.choice_avg_gap - rule.choice_avg_gap
        status = "pass"
        if row.avg < rule.avg:
            status = "fail-avg"
        if row.p95 < rule.p95:
            status = "fail-p95" if status == "pass" else f"{status}+p95"
        if row.choice_match_rate < rule.choice_match_rate:
            status = "fail-choice-match" if status == "pass" else f"{status}+choice-match"
        if row.choice_avg_gap > rule.choice_avg_gap:
            status = "fail-choice-gap" if status == "pass" else f"{status}+choice-gap"
        compare_rows.append(
            [
                row.case_id,
                row.mode,
                row.level,
                row.seed,
                format_f3(rule.avg),
                format_f3(row.avg),
                format_f3(delta_avg),
                str(rule.p95),
                str(row.p95),
                str(delta_p95),
                format_f3(rule.choice_match_rate),
                format_f3(row.choice_match_rate),
                format_f3(delta_choice_match),
                format_f3(rule.choice_avg_gap),
                format_f3(row.choice_avg_gap),
                format_f3(delta_choice_gap),
                status,
            ]
        )

    compare_file.parent.mkdir(parents=True, exist_ok=True)
    with compare_file.open("w", encoding="utf-8", newline="") as f:
        w = csv.writer(f, delimiter="\t")
        w.writerow(
            [
                "id",
                "mode",
                "level",
                "seed",
                "rule_avg",
                "ml_avg",
                "delta_avg",
                "rule_p95",
                "ml_p95",
                "delta_p95",
                "rule_choice_match",
                "ml_choice_match",
                "delta_choice_match",
                "rule_choice_gap",
                "ml_choice_gap",
                "delta_choice_gap",
                "status",
            ]
        )
        w.writerows(compare_rows)

    print(f"[bot-leaderboard] rows: {result_file}")
    print(result_file.read_text(encoding="utf-8"), end="")
    print(f"[bot-leaderboard] summary: {summary_file}")
    print(summary_file.read_text(encoding="utf-8"), end="")
    print(f"[bot-leaderboard] compare: {compare_file}")
    print(compare_file.read_text(encoding="utf-8"), end="")

    if args.require_no_regression == 1 and any(row[-1] != "pass" for row in compare_rows):
        print("[bot-leaderboard] no-regression gate failed", file=sys.stderr)
        return 1
    if args.require_no_regression == 1:
        print("[bot-leaderboard] no-regression gate passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
