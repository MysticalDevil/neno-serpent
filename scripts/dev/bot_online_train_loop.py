#!/usr/bin/env python3
"""Continuous online-training loop for ml-online backend.

This module holds the heavy control-flow and metrics logic so shell entrypoints
stay thin and focused on orchestration.
"""

from __future__ import annotations

import argparse
import hashlib
import re
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path

SCORE_LINE_RE = re.compile(r"^\[bot-benchmark\] score\.max=.*$", re.MULTILINE)
AVG_RE = re.compile(r"score\.avg=([0-9]+(?:\.[0-9]+)?)")
P95_RE = re.compile(r"score\.p95=([0-9]+(?:\.[0-9]+)?)")


def run_cmd(cmd: list[str], allow_fail: bool = False) -> str:
    result = subprocess.run(cmd, text=True, capture_output=True, check=False)
    if result.returncode != 0 and not allow_fail:
        raise RuntimeError(
            f"command failed ({result.returncode}): {' '.join(cmd)}\n"
            f"stdout:\n{result.stdout}\n"
            f"stderr:\n{result.stderr}"
        )
    return result.stdout + result.stderr


def parse_score_pair(text: str) -> tuple[float, float]:
    line_match = SCORE_LINE_RE.search(text)
    if line_match is None:
        return float("nan"), float("nan")
    line = line_match.group(0)
    avg_match = AVG_RE.search(line)
    p95_match = P95_RE.search(line)
    if avg_match is None or p95_match is None:
        return float("nan"), float("nan")
    return float(avg_match.group(1)), float(p95_match.group(1))


def file_sha256_or_na(path: Path) -> str:
    if not path.is_file():
        return "na"
    h = hashlib.sha256()
    with path.open("rb") as f:
        while True:
            chunk = f.read(65536)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def dataset_sample_count(path: Path) -> int:
    if not path.is_file():
        return 0
    line_count = 0
    with path.open("r", encoding="utf-8") as f:
        for _ in f:
            line_count += 1
    return max(0, line_count - 1)


def workspace_size_mb(path: Path) -> int:
    total = 0
    for child in path.rglob("*"):
        if child.is_file():
            total += child.stat().st_size
    return int((total + (1024 * 1024 - 1)) / (1024 * 1024))


def prune_workspace_if_needed(
    workspace: Path,
    cache_max_mb: int,
    cache_target_mb: int,
    runtime_json: Path,
) -> None:
    current_mb = workspace_size_mb(workspace)
    if current_mb < cache_max_mb:
        return
    print(
        f"[bot-online-train] cache high-water hit sizeMb={current_mb} "
        f"maxMb={cache_max_mb}; pruning to <={cache_target_mb}MB"
    )
    while current_mb > cache_target_mb:
        candidates = []
        for p in workspace.iterdir():
            if not p.is_file():
                continue
            if p.name in {".gitignore", runtime_json.name}:
                continue
            candidates.append(p)
        if not candidates:
            print("[bot-online-train] prune stopped: no removable files in workspace")
            break
        oldest = min(candidates, key=lambda p: p.stat().st_mtime)
        oldest.unlink(missing_ok=True)
        current_mb = workspace_size_mb(workspace)
        print(f"[bot-online-train] pruned file={oldest} sizeMb={current_mb}")


def ensure_publish_history_header(path: Path) -> None:
    if path.exists():
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        "round\tpublished\treason\tdataset_samples\tbaseline_avg\tbaseline_p95\t"
        "candidate_avg\tcandidate_p95\tcandidate_sha256\tpublished_sha256\n",
        encoding="utf-8",
    )


def append_publish_history_row(
    path: Path,
    round_idx: int,
    published: bool,
    reason: str,
    dataset_samples: int,
    baseline_avg: str,
    baseline_p95: str,
    candidate_avg: str,
    candidate_p95: str,
    candidate_sha: str,
    published_sha: str,
) -> None:
    with path.open("a", encoding="utf-8") as f:
        f.write(
            f"{round_idx}\t{1 if published else 0}\t{reason}\t{dataset_samples}\t"
            f"{baseline_avg}\t{baseline_p95}\t{candidate_avg}\t{candidate_p95}\t"
            f"{candidate_sha}\t{published_sha}\n"
        )


@dataclass
class LoopContext:
    round_idx: int = 0
    published_count: int = 0
    blocked_count: int = 0
    blocked_streak: int = 0
    max_blocked_streak_seen: int = 0
    last_gate_reason: str = "none"
    last_publish_round: int = 0


def format_metric(value: float) -> str:
    if value != value:  # NaN
        return "nan"
    return f"{value:.6g}"


def main() -> int:
    parser = argparse.ArgumentParser(description="NenoSerpent ml-online training loop")
    parser.add_argument("--root-dir", required=True)
    parser.add_argument("--workspace", required=True)
    parser.add_argument("--profile", required=True)
    parser.add_argument("--suite", required=True)
    parser.add_argument("--interval-sec", type=int, required=True)
    parser.add_argument("--max-samples-per-case", type=int, required=True)
    parser.add_argument("--epochs", type=int, required=True)
    parser.add_argument("--batch-size", type=int, required=True)
    parser.add_argument("--lr", type=float, required=True)
    parser.add_argument("--seed-base", type=int, required=True)
    parser.add_argument("--gate-games", type=int, required=True)
    parser.add_argument("--gate-max-ticks", type=int, required=True)
    parser.add_argument("--gate-level", type=int, required=True)
    parser.add_argument("--gate-mode", required=True)
    parser.add_argument("--gate-eps", type=float, required=True)
    parser.add_argument("--cache-max-mb", type=int, required=True)
    parser.add_argument("--cache-target-mb", type=int, required=True)
    parser.add_argument("--max-rounds", type=int, required=True)
    parser.add_argument("--summary", default="")
    parser.add_argument("--publish-history", required=True)
    parser.add_argument("--min-dataset-samples", type=int, required=True)
    parser.add_argument("--min-publish-round", type=int, required=True)
    parser.add_argument("--publish-cooldown-rounds", type=int, required=True)
    parser.add_argument("--max-blocked-streak", type=int, required=True)
    args = parser.parse_args()

    root = Path(args.root_dir).resolve()
    dev_sh = root / "scripts" / "dev.sh"
    workspace = Path(args.workspace).resolve()
    workspace.mkdir(parents=True, exist_ok=True)
    dataset_path = workspace / "nenoserpent_bot_dataset.csv"
    model_path = workspace / "nenoserpent_bot_policy.pt"
    metadata_path = workspace / "nenoserpent_bot_policy_meta.json"
    runtime_json = workspace / "nenoserpent_bot_policy_runtime.json"
    publish_history = Path(args.publish_history).resolve()
    summary_path = Path(args.summary).resolve() if args.summary else None
    ensure_publish_history_header(publish_history)
    ctx = LoopContext()

    print(f"[bot-online-train] workspace={workspace}")
    print(
        f"[bot-online-train] profile={args.profile} intervalSec={args.interval_sec} "
        f"epochs={args.epochs} batchSize={args.batch_size}"
    )
    print(f"[bot-online-train] datasetSuite={args.suite}")
    print(
        f"[bot-online-train] gate games={args.gate_games} maxTicks={args.gate_max_ticks} "
        f"level={args.gate_level} mode={args.gate_mode} eps={args.gate_eps}"
    )
    print(
        f"[bot-online-train] cache maxMb={args.cache_max_mb} targetMb={args.cache_target_mb}"
    )
    print(
        f"[bot-online-train] maxRounds={args.max_rounds} "
        f"summary={(str(summary_path) if summary_path else '<none>')}"
    )
    print(
        "[bot-online-train] stability "
        f"minSamples={args.min_dataset_samples} minPublishRound={args.min_publish_round} "
        f"cooldownRounds={args.publish_cooldown_rounds} maxBlockedStreak={args.max_blocked_streak}"
    )
    print(f"[bot-online-train] publishHistory={publish_history}")

    while True:
        ctx.round_idx += 1
        round_seed = args.seed_base + ctx.round_idx
        tmp_runtime_json = workspace / f".runtime.round-{ctx.round_idx}.json"
        baseline_avg = float("nan")
        baseline_p95 = float("nan")
        candidate_avg = float("nan")
        candidate_p95 = float("nan")

        print(f"[bot-online-train] round={ctx.round_idx} phase=dataset")
        run_cmd(
            [
                str(dev_sh),
                "bot-dataset",
                "--profile",
                args.profile,
                "--suite",
                args.suite,
                "--max-samples-per-case",
                str(args.max_samples_per_case),
                "--output",
                str(dataset_path),
            ]
        )
        sample_count = dataset_sample_count(dataset_path)
        print(f"[bot-online-train] round={ctx.round_idx} dataset.samples={sample_count}")

        should_publish = True
        if sample_count < args.min_dataset_samples:
            should_publish = False
            ctx.last_gate_reason = "insufficient-samples"
            print(
                f"[bot-online-train] round={ctx.round_idx} gate=blocked "
                f"reason=insufficient-samples samples={sample_count} min={args.min_dataset_samples}"
            )
        if ctx.round_idx < args.min_publish_round:
            should_publish = False
            ctx.last_gate_reason = "warmup-round"
            print(
                f"[bot-online-train] round={ctx.round_idx} gate=blocked "
                f"reason=warmup-round minRound={args.min_publish_round}"
            )
        if ctx.last_publish_round > 0 and args.publish_cooldown_rounds > 0:
            rounds_since_publish = ctx.round_idx - ctx.last_publish_round
            if rounds_since_publish < args.publish_cooldown_rounds:
                should_publish = False
                ctx.last_gate_reason = "publish-cooldown"
                print(
                    f"[bot-online-train] round={ctx.round_idx} gate=blocked "
                    f"reason=publish-cooldown since={rounds_since_publish} "
                    f"need={args.publish_cooldown_rounds}"
                )

        print(f"[bot-online-train] round={ctx.round_idx} phase=train seed={round_seed}")
        run_cmd(
            [
                str(dev_sh),
                "bot-train",
                "--dataset",
                str(dataset_path),
                "--model",
                str(model_path),
                "--metadata",
                str(metadata_path),
                "--runtime-json",
                str(tmp_runtime_json),
                "--epochs",
                str(args.epochs),
                "--batch-size",
                str(args.batch_size),
                "--lr",
                str(args.lr),
                "--seed",
                str(round_seed),
            ]
        )

        if runtime_json.is_file():
            print(
                f"[bot-online-train] round={ctx.round_idx} "
                "phase=gate baseline=current candidate=new"
            )
            baseline_out = run_cmd(
                [
                    str(dev_sh),
                    "bot-benchmark",
                    "--profile",
                    args.profile,
                    "--mode",
                    args.gate_mode,
                    "--backend",
                    "ml",
                    "--ml-model",
                    str(runtime_json),
                    "--games",
                    str(args.gate_games),
                    "--max-ticks",
                    str(args.gate_max_ticks),
                    "--level",
                    str(args.gate_level),
                    "--seed",
                    str(round_seed),
                ],
                allow_fail=True,
            )
            candidate_out = run_cmd(
                [
                    str(dev_sh),
                    "bot-benchmark",
                    "--profile",
                    args.profile,
                    "--mode",
                    args.gate_mode,
                    "--backend",
                    "ml",
                    "--ml-model",
                    str(tmp_runtime_json),
                    "--games",
                    str(args.gate_games),
                    "--max-ticks",
                    str(args.gate_max_ticks),
                    "--level",
                    str(args.gate_level),
                    "--seed",
                    str(round_seed),
                ],
                allow_fail=True,
            )
            baseline_avg, baseline_p95 = parse_score_pair(baseline_out)
            candidate_avg, candidate_p95 = parse_score_pair(candidate_out)
            print(
                f"[bot-online-train] round={ctx.round_idx} gate "
                f"baseline.avg={format_metric(baseline_avg)} "
                f"baseline.p95={format_metric(baseline_p95)} "
                f"candidate.avg={format_metric(candidate_avg)} "
                f"candidate.p95={format_metric(candidate_p95)}"
            )
            if candidate_avg + args.gate_eps < baseline_avg:
                should_publish = False
                ctx.last_gate_reason = "avg-regression"
                print(
                    f"[bot-online-train] round={ctx.round_idx} "
                    "gate=blocked reason=avg-regression"
                )
            if candidate_p95 + args.gate_eps < baseline_p95:
                should_publish = False
                ctx.last_gate_reason = "p95-regression"
                print(
                    f"[bot-online-train] round={ctx.round_idx} "
                    "gate=blocked reason=p95-regression"
                )

        if should_publish:
            tmp_runtime_json.replace(runtime_json)
            ctx.published_count += 1
            ctx.last_gate_reason = "published"
            ctx.last_publish_round = ctx.round_idx
            ctx.blocked_streak = 0
            candidate_sha = file_sha256_or_na(runtime_json)
            append_publish_history_row(
                publish_history,
                ctx.round_idx,
                True,
                ctx.last_gate_reason,
                sample_count,
                format_metric(baseline_avg),
                format_metric(baseline_p95),
                format_metric(candidate_avg),
                format_metric(candidate_p95),
                candidate_sha,
                candidate_sha,
            )
            print(f"[bot-online-train] round={ctx.round_idx} published={runtime_json}")
        else:
            candidate_sha = file_sha256_or_na(tmp_runtime_json)
            tmp_runtime_json.unlink(missing_ok=True)
            ctx.blocked_count += 1
            ctx.blocked_streak += 1
            ctx.max_blocked_streak_seen = max(
                ctx.max_blocked_streak_seen, ctx.blocked_streak
            )
            append_publish_history_row(
                publish_history,
                ctx.round_idx,
                False,
                ctx.last_gate_reason,
                sample_count,
                format_metric(baseline_avg),
                format_metric(baseline_p95),
                format_metric(candidate_avg),
                format_metric(candidate_p95),
                candidate_sha,
                file_sha256_or_na(runtime_json),
            )
            print(f"[bot-online-train] round={ctx.round_idx} kept-current={runtime_json}")

        prune_workspace_if_needed(
            workspace=workspace,
            cache_max_mb=args.cache_max_mb,
            cache_target_mb=args.cache_target_mb,
            runtime_json=runtime_json,
        )
        if args.max_blocked_streak > 0 and ctx.blocked_streak >= args.max_blocked_streak:
            ctx.last_gate_reason = "blocked-streak-limit"
            print(
                f"[bot-online-train] stop reason=blocked-streak-limit "
                f"streak={ctx.blocked_streak} limit={args.max_blocked_streak}"
            )
            break
        if args.max_rounds > 0 and ctx.round_idx >= args.max_rounds:
            break
        time.sleep(args.interval_sec)

    if summary_path is not None:
        summary_path.parent.mkdir(parents=True, exist_ok=True)
        summary_path.write_text(
            "\n".join(
                [
                    f"rounds={ctx.round_idx}",
                    f"published={ctx.published_count}",
                    f"blocked={ctx.blocked_count}",
                    f"runtime_json={runtime_json}",
                    f"last_reason={ctx.last_gate_reason}",
                    f"last_publish_round={ctx.last_publish_round}",
                    f"max_blocked_streak={ctx.max_blocked_streak_seen}",
                    f"publish_history={publish_history}",
                ]
            )
            + "\n",
            encoding="utf-8",
        )
        print(f"[bot-online-train] summary={summary_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
