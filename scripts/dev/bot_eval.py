#!/usr/bin/env python3
"""Evaluate trained imitation model on a dataset CSV."""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path

try:
    import torch
    from torch import nn
except ImportError as exc:  # pragma: no cover
    raise SystemExit(f"PyTorch is required for bot_eval.py: {exc}") from exc

FEATURE_COLUMNS = [
    "level",
    "score",
    "body_len",
    "head_x",
    "head_y",
    "dir_x",
    "dir_y",
    "food_dx",
    "food_dy",
    "power_dx",
    "power_dy",
    "power_type",
    "power_active",
    "ghost_active",
    "shield_active",
    "portal_active",
    "laser_active",
    "danger_up",
    "danger_right",
    "danger_down",
    "danger_left",
]


class MlpPolicy(nn.Module):
    def __init__(self, input_dim: int) -> None:
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(input_dim, 64),
            nn.ReLU(),
            nn.Linear(64, 64),
            nn.ReLU(),
            nn.Linear(64, 4),
        )

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        return self.net(x)


def load_dataset(path: Path) -> tuple[torch.Tensor, torch.Tensor]:
    xs: list[list[float]] = []
    ys: list[int] = []
    with path.open("r", encoding="utf-8", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            xs.append([float(row[c]) for c in FEATURE_COLUMNS])
            ys.append(int(row["action"]))
    return torch.tensor(xs, dtype=torch.float32), torch.tensor(ys, dtype=torch.long)


def main() -> int:
    parser = argparse.ArgumentParser(description="Evaluate imitation model on dataset CSV.")
    parser.add_argument("--dataset", required=True)
    parser.add_argument("--model", required=True)
    parser.add_argument("--report", default="/tmp/nenoserpent_bot_eval_report.json")
    args = parser.parse_args()

    ckpt = torch.load(Path(args.model).resolve(), map_location="cpu", weights_only=False)
    model = MlpPolicy(input_dim=len(FEATURE_COLUMNS))
    model.load_state_dict(ckpt["model_state_dict"])
    model.eval()

    x, y = load_dataset(Path(args.dataset).resolve())
    mean = ckpt["feature_mean"]
    std = ckpt["feature_std"]
    x = (x - mean) / std

    with torch.no_grad():
      logits = model(x)
    pred = logits.argmax(dim=1)
    acc = float((pred == y).float().mean().item())

    report = {
        "dataset": str(Path(args.dataset).resolve()),
        "model": str(Path(args.model).resolve()),
        "samples": int(y.shape[0]),
        "accuracy": acc,
    }
    report_path = Path(args.report).resolve()
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"[bot-eval] samples={report['samples']} accuracy={acc:.4f}")
    print(f"[bot-eval] report={report_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
