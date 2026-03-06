#!/usr/bin/env python3
"""Train an imitation-learning policy from bot dataset CSV.

This script trains a small MLP classifier that predicts action classes:
0=up, 1=right, 2=down, 3=left.
"""

from __future__ import annotations

import argparse
import csv
import json
import random
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

try:
    import torch
    from torch import nn
except ImportError as exc:  # pragma: no cover - runtime dependency check
    raise SystemExit(f"PyTorch is required for bot_train.py: {exc}") from exc


FEATURE_COLUMNS_V2 = [
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
LABEL_COLUMN = "action"


@dataclass
class Dataset:
    x: torch.Tensor
    y: torch.Tensor
    sample_weights: torch.Tensor
    seed_values: Optional[list[int]]


class MlpPolicy(nn.Module):
    def __init__(self, input_dim: int) -> None:
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(input_dim, 128),
            nn.ReLU(),
            nn.Linear(128, 128),
            nn.ReLU(),
            nn.Linear(128, 64),
            nn.ReLU(),
            nn.Linear(64, 4),
        )

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        return self.net(x)


def hard_sample_weights(x: torch.Tensor, y: torch.Tensor, scale: float) -> torch.Tensor:
    danger_up = x[:, FEATURE_COLUMNS_V2.index("danger_up")]
    danger_right = x[:, FEATURE_COLUMNS_V2.index("danger_right")]
    danger_down = x[:, FEATURE_COLUMNS_V2.index("danger_down")]
    danger_left = x[:, FEATURE_COLUMNS_V2.index("danger_left")]
    body_len = x[:, FEATURE_COLUMNS_V2.index("body_len")]
    score = x[:, FEATURE_COLUMNS_V2.index("score")]

    danger_sum = danger_up + danger_right + danger_down + danger_left
    per_action_danger = torch.zeros_like(danger_sum)
    per_action_danger = torch.where(y == 0, danger_up, per_action_danger)
    per_action_danger = torch.where(y == 1, danger_right, per_action_danger)
    per_action_danger = torch.where(y == 2, danger_down, per_action_danger)
    per_action_danger = torch.where(y == 3, danger_left, per_action_danger)

    long_body = (body_len >= 18).float()
    high_score = (score >= 60).float() + (score >= 120).float()
    weights = 1.0 + scale * (
        1.8 * per_action_danger + 0.35 * danger_sum + 0.45 * long_body + 0.30 * high_score
    )
    return torch.clamp(weights, min=0.1, max=8.0)


def load_csv(path: Path, hard_sample_scale: float) -> Dataset:
    rows_x: list[list[float]] = []
    rows_y: list[int] = []
    seed_values: list[int] = []
    has_seed_column = False
    with path.open("r", encoding="utf-8", newline="") as f:
        reader = csv.DictReader(f)
        missing = [c for c in FEATURE_COLUMNS_V2 + [LABEL_COLUMN] if c not in reader.fieldnames]
        if missing:
            raise RuntimeError(f"dataset missing required columns: {missing}")
        has_seed_column = bool(reader.fieldnames and "seed" in reader.fieldnames)
        for row in reader:
            rows_x.append([float(row[c]) for c in FEATURE_COLUMNS_V2])
            rows_y.append(int(row[LABEL_COLUMN]))
            if has_seed_column:
                seed_values.append(int(row["seed"]))
    if not rows_x:
        raise RuntimeError(f"empty dataset: {path}")
    x = torch.tensor(rows_x, dtype=torch.float32)
    y = torch.tensor(rows_y, dtype=torch.long)
    return Dataset(
        x=x,
        y=y,
        sample_weights=hard_sample_weights(x, y, hard_sample_scale),
        seed_values=seed_values if has_seed_column else None,
    )


def split_dataset(dataset: Dataset, train_ratio: float, seed: int) -> tuple[Dataset, Dataset]:
    n = dataset.x.shape[0]
    if dataset.seed_values is not None and len(set(dataset.seed_values)) > 1:
        groups: dict[int, list[int]] = {}
        for idx, seed_value in enumerate(dataset.seed_values):
            groups.setdefault(seed_value, []).append(idx)
        seed_keys = list(groups.keys())
        random.Random(seed).shuffle(seed_keys)
        train_group_count = max(1, min(len(seed_keys) - 1, int(len(seed_keys) * train_ratio)))
        train_group_keys = set(seed_keys[:train_group_count])
        train_rows: list[int] = []
        val_rows: list[int] = []
        for seed_key, rows in groups.items():
            if seed_key in train_group_keys:
                train_rows.extend(rows)
            else:
                val_rows.extend(rows)
        if not train_rows or not val_rows:
            idx = list(range(n))
            random.Random(seed).shuffle(idx)
            train_n = max(1, min(n - 1, int(n * train_ratio)))
            train_rows = idx[:train_n]
            val_rows = idx[train_n:]
        train_idx = torch.tensor(train_rows, dtype=torch.long)
        val_idx = torch.tensor(val_rows, dtype=torch.long)
    else:
        idx = list(range(n))
        random.Random(seed).shuffle(idx)
        train_n = max(1, min(n - 1, int(n * train_ratio)))
        train_idx = torch.tensor(idx[:train_n], dtype=torch.long)
        val_idx = torch.tensor(idx[train_n:], dtype=torch.long)
    return (
        Dataset(
            x=dataset.x[train_idx],
            y=dataset.y[train_idx],
            sample_weights=dataset.sample_weights[train_idx],
            seed_values=None,
        ),
        Dataset(
            x=dataset.x[val_idx],
            y=dataset.y[val_idx],
            sample_weights=dataset.sample_weights[val_idx],
            seed_values=None,
        ),
    )


def zscore_fit(x: torch.Tensor) -> tuple[torch.Tensor, torch.Tensor]:
    mean = x.mean(dim=0)
    std = x.std(dim=0, unbiased=False)
    std = torch.where(std < 1e-6, torch.ones_like(std), std)
    return mean, std


def zscore_apply(x: torch.Tensor, mean: torch.Tensor, std: torch.Tensor) -> torch.Tensor:
    return (x - mean) / std


def accuracy(logits: torch.Tensor, labels: torch.Tensor) -> float:
    pred = logits.argmax(dim=1)
    return float((pred == labels).float().mean().item())


def run_epoch(
    model: nn.Module,
    optimizer: torch.optim.Optimizer,
    x: torch.Tensor,
    y: torch.Tensor,
    sample_weights: Optional[torch.Tensor],
    batch_size: int,
    train: bool,
) -> tuple[float, float]:
    criterion = nn.CrossEntropyLoss()
    n = x.shape[0]
    if train and sample_weights is not None:
        normalized_weights = sample_weights / sample_weights.sum()
        permutation = torch.multinomial(normalized_weights, n, replacement=True)
    else:
        permutation = torch.randperm(n) if train else torch.arange(n)
    total_loss = 0.0
    total_acc = 0.0
    batches = 0
    model.train(train)
    for i in range(0, n, batch_size):
        idx = permutation[i : i + batch_size]
        xb = x[idx]
        yb = y[idx]
        logits = model(xb)
        loss = criterion(logits, yb)
        if train:
            optimizer.zero_grad()
            loss.backward()
            optimizer.step()
        total_loss += float(loss.item())
        total_acc += accuracy(logits, yb)
        batches += 1
    return total_loss / max(1, batches), total_acc / max(1, batches)


def main() -> int:
    parser = argparse.ArgumentParser(description="Train MLP imitation policy from bot CSV dataset.")
    parser.add_argument("--dataset", required=True, help="CSV generated by bot dataset script")
    parser.add_argument("--output-model", required=True, help="Output .pt file")
    parser.add_argument("--output-metadata", required=True, help="Output metadata JSON")
    parser.add_argument(
        "--output-runtime-json",
        required=False,
        help="Optional runtime JSON for C++ ML backend inference",
    )
    parser.add_argument("--epochs", type=int, default=30)
    parser.add_argument("--batch-size", type=int, default=256)
    parser.add_argument("--lr", type=float, default=1e-3)
    parser.add_argument("--train-ratio", type=float, default=0.9)
    parser.add_argument("--seed", type=int, default=20260304)
    parser.add_argument(
        "--hard-sample-scale",
        type=float,
        default=1.0,
        help="Scale for hard-sample weighted sampling during training",
    )
    args = parser.parse_args()

    random.seed(args.seed)
    torch.manual_seed(args.seed)

    dataset = load_csv(Path(args.dataset), hard_sample_scale=args.hard_sample_scale)
    train_data, val_data = split_dataset(dataset, args.train_ratio, args.seed)

    mean, std = zscore_fit(train_data.x)
    train_x = zscore_apply(train_data.x, mean, std)
    val_x = zscore_apply(val_data.x, mean, std)

    model = MlpPolicy(input_dim=train_x.shape[1])
    optimizer = torch.optim.AdamW(model.parameters(), lr=args.lr)

    best_val_acc = -1.0
    best_state = None
    history: list[dict] = []
    for epoch in range(1, args.epochs + 1):
        train_loss, train_acc = run_epoch(
            model=model,
            optimizer=optimizer,
            x=train_x,
            y=train_data.y,
            sample_weights=train_data.sample_weights,
            batch_size=max(1, args.batch_size),
            train=True,
        )
        with torch.no_grad():
            val_loss, val_acc = run_epoch(
                model=model,
                optimizer=optimizer,
                x=val_x,
                y=val_data.y,
                sample_weights=None,
                batch_size=max(1, args.batch_size),
                train=False,
            )
        history.append(
            {
                "epoch": epoch,
                "train_loss": train_loss,
                "train_acc": train_acc,
                "val_loss": val_loss,
                "val_acc": val_acc,
            }
        )
        print(
            f"[bot-train] epoch={epoch:03d} train.loss={train_loss:.4f} train.acc={train_acc:.4f} "
            f"val.loss={val_loss:.4f} val.acc={val_acc:.4f}"
        )
        if val_acc > best_val_acc:
            best_val_acc = val_acc
            best_state = {
                "model_state_dict": model.state_dict(),
                "feature_mean": mean,
                "feature_std": std,
            }

    if best_state is None:
        raise RuntimeError("training failed to produce model state")

    output_model = Path(args.output_model).resolve()
    output_model.parent.mkdir(parents=True, exist_ok=True)
    torch.save(best_state, output_model)

    output_metadata = Path(args.output_metadata).resolve()
    output_metadata.parent.mkdir(parents=True, exist_ok=True)
    metadata = {
        "dataset": str(Path(args.dataset).resolve()),
        "model": str(output_model),
        "input_dim": len(FEATURE_COLUMNS_V2),
        "labels": {"0": "up", "1": "right", "2": "down", "3": "left"},
        "epochs": args.epochs,
        "batch_size": args.batch_size,
        "lr": args.lr,
        "train_ratio": args.train_ratio,
        "seed": args.seed,
        "hard_sample_scale": args.hard_sample_scale,
        "dataset_seed_grouped_split": dataset.seed_values is not None,
        "best_val_acc": best_val_acc,
        "history_tail": history[-10:],
        "feature_columns_v2": FEATURE_COLUMNS_V2,
    }
    output_metadata.write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")

    if args.output_runtime_json:
        runtime_json_path = Path(args.output_runtime_json).resolve()
        runtime_json_path.parent.mkdir(parents=True, exist_ok=True)

        state_dict = best_state["model_state_dict"]
        mean_vec = best_state["feature_mean"].tolist()
        std_vec = best_state["feature_std"].tolist()

        def layer_payload(weight_key: str, bias_key: str, activation: str) -> dict:
            weight = state_dict[weight_key].detach().cpu().tolist()
            bias = state_dict[bias_key].detach().cpu().tolist()
            flat_weight = [float(value) for row in weight for value in row]
            return {
                "input_dim": len(weight[0]),
                "output_dim": len(weight),
                "activation": activation,
                "weights": flat_weight,
                "bias": [float(v) for v in bias],
            }

        runtime_payload = {
            "format": "nenoserpent-bot-mlp-v2",
            "normalization": {
                "mean": [float(v) for v in mean_vec],
                "std": [float(v) for v in std_vec],
            },
            "feature_columns_v2": FEATURE_COLUMNS_V2,
            "layers": [
                layer_payload("net.0.weight", "net.0.bias", "relu"),
                layer_payload("net.2.weight", "net.2.bias", "relu"),
                layer_payload("net.4.weight", "net.4.bias", "relu"),
                layer_payload("net.6.weight", "net.6.bias", "none"),
            ],
            "hybrid": {
                "logit_weight": 1.0,
                "risk_weight": 0.9,
                "loop_weight": 0.8,
                "orbit_weight": 1.15,
                "stall_weight": 0.45,
                "progress_weight": 0.45,
                "food_weight": 0.35,
                "space_weight": 0.16,
                "safe_neighbor_weight": 0.12,
                "hash_window": 192,
                "tie_break_seed": 17,
            },
        }
        metadata["recommended_min_conf"] = 0.55
        metadata["recommended_min_margin"] = 0.10
        runtime_json_path.write_text(
            json.dumps(runtime_payload, indent=2) + "\n", encoding="utf-8"
        )
        metadata["runtime_json"] = str(runtime_json_path)
        output_metadata.write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
        print(f"[bot-train] runtime_json={runtime_json_path}")

    print(f"[bot-train] model={output_model}")
    print(f"[bot-train] metadata={output_metadata}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
