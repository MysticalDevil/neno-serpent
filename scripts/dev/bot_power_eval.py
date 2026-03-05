#!/usr/bin/env python3
"""Evaluate power-up chase decision quality."""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path


def safe_int(value: str, default: int = -1) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def main() -> int:
    parser = argparse.ArgumentParser(description="Evaluate power-up chase dataset quality.")
    parser.add_argument("--dataset", required=True)
    parser.add_argument("--report", required=True)
    parser.add_argument("--summary-env")
    args = parser.parse_args()

    total = 0
    top1 = 0
    top2 = 0
    rank_sum = 0.0
    by_type: dict[int, dict[str, float]] = {}

    with Path(args.dataset).open("r", encoding="utf-8", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rank = safe_int(row.get("selected_rank", "-1"))
            power_type = safe_int(row.get("power_type_runtime", row.get("power_type", "-1")))
            if rank <= 0:
                continue
            total += 1
            rank_sum += rank
            if rank <= 1:
                top1 += 1
            if rank <= 2:
                top2 += 1
            bucket = by_type.setdefault(power_type, {"samples": 0.0, "top1": 0.0, "top2": 0.0})
            bucket["samples"] += 1.0
            if rank <= 1:
                bucket["top1"] += 1.0
            if rank <= 2:
                bucket["top2"] += 1.0

    top1_acc = (top1 / total) if total > 0 else 0.0
    top2_acc = (top2 / total) if total > 0 else 0.0
    avg_rank = (rank_sum / total) if total > 0 else 0.0

    by_type_report: dict[str, dict[str, float]] = {}
    for power_type, bucket in sorted(by_type.items(), key=lambda item: item[0]):
        samples = int(bucket["samples"])
        by_type_report[str(power_type)] = {
            "samples": samples,
            "top1_acc": (bucket["top1"] / samples) if samples > 0 else 0.0,
            "top2_acc": (bucket["top2"] / samples) if samples > 0 else 0.0,
        }

    report = {
        "dataset": str(Path(args.dataset).resolve()),
        "samples": total,
        "top1_acc": top1_acc,
        "top2_acc": top2_acc,
        "avg_rank": avg_rank,
        "by_power_type": by_type_report,
    }

    report_path = Path(args.report).resolve()
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")

    if args.summary_env:
        summary_path = Path(args.summary_env).resolve()
        summary_path.parent.mkdir(parents=True, exist_ok=True)
        summary_path.write_text(
            "\n".join(
                [
                    f"samples={total}",
                    f"top1_acc={top1_acc:.6f}",
                    f"top2_acc={top2_acc:.6f}",
                    f"avg_rank={avg_rank:.6f}",
                    "",
                ]
            ),
            encoding="utf-8",
        )

    print(
        f"[bot-power-eval] samples={total} top1_acc={top1_acc:.4f} top2_acc={top2_acc:.4f} "
        f"avg_rank={avg_rank:.4f}"
    )
    print(f"[bot-power-eval] report={report_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
