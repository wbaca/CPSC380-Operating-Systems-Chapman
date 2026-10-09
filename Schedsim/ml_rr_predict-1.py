#!/usr/bin/env python3
"""
ml_rr_predict.py

Train a simple ML model on rr_training_data.csv and predict a
recommended Round-Robin time quantum for a new workload file.

Expected training CSV columns:
    num_processes,avg_burst,max_burst,min_burst,std_burst,avg_arrival_gap,optimal_quantum

Workload file format:
    PID Arrival Burst Priority
    P1  0       8     3
    P2  1       4     1
    ...

Usage:
    python ml_rr_predict.py rr_training_data.csv
    python ml_rr_predict.py rr_training_data.csv new_tasks.txt
"""

from __future__ import annotations

import csv
import math
import sys
from collections import Counter
from pathlib import Path
from typing import Dict, Iterable, List, Tuple

try:
    import pandas as pd
    from sklearn.metrics import accuracy_score
    from sklearn.model_selection import train_test_split
    from sklearn.tree import DecisionTreeClassifier
except Exception as exc:  # pragma: no cover
    print("ERROR: Missing dependency. Install with:")
    print("  pip install pandas scikit-learn")
    print(f"Details: {exc}")
    sys.exit(1)


FEATURE_COLUMNS = [
    "num_processes",
    "avg_burst",
    "max_burst",
    "min_burst",
    "std_burst",
    "avg_arrival_gap",
]
LABEL_COLUMN = "optimal_quantum"


def usage() -> None:
    print("Usage:")
    print("  python ml_rr_predict.py rr_training_data.csv [new_workload.txt]")


def parse_workload(workload_path: Path) -> Dict[str, float]:
    
    bursts: List[float] = []
    arrivals: List[float] = []

    with workload_path.open("r", encoding="utf-8") as f:
        for raw_line in f:
            line = raw_line.strip()
            if not line:
                continue

            parts = line.split()
            if parts[0].lower() == "pid":
                continue

            if len(parts) < 4:
                raise ValueError(
                    f"Malformed workload line in {workload_path}: {raw_line.rstrip()}"
                )

            try:
                arrival = float(parts[1])
                burst = float(parts[2])
            except ValueError as exc:
                raise ValueError(
                    f"Non-numeric arrival/burst in {workload_path}: {raw_line.rstrip()}"
                ) from exc

            arrivals.append(arrival)
            bursts.append(burst)

    if not bursts:
        raise ValueError(f"No processes found in workload file: {workload_path}")

    arrivals_sorted = sorted(arrivals)
    if len(arrivals_sorted) <= 1:
        avg_arrival_gap = 0.0
    else:
        gaps = [
            arrivals_sorted[i] - arrivals_sorted[i - 1]
            for i in range(1, len(arrivals_sorted))
        ]
        avg_arrival_gap = sum(gaps) / len(gaps)

    avg_burst = sum(bursts) / len(bursts)
    variance = sum((b - avg_burst) ** 2 for b in bursts) / len(bursts)
    std_burst = math.sqrt(variance)

    return {
        "num_processes": float(len(bursts)),
        "avg_burst": float(avg_burst),
        "max_burst": float(max(bursts)),
        "min_burst": float(min(bursts)),
        "std_burst": float(std_burst),
        "avg_arrival_gap": float(avg_arrival_gap),
    }


def load_training_data(csv_path: Path) -> pd.DataFrame:
    df = pd.read_csv(csv_path)

    missing = [c for c in FEATURE_COLUMNS + [LABEL_COLUMN] if c not in df.columns]
    if missing:
        raise ValueError(
            "Training CSV is missing required columns: " + ", ".join(missing)
        )

    if df.empty:
        raise ValueError("Training CSV is empty.")

    return df


def summarize_training_data(df: pd.DataFrame) -> None:
    labels = Counter(df[LABEL_COLUMN].tolist())
    print(f"Training rows: {len(df)}")
    print("Quantum distribution:")
    for q, count in sorted(labels.items(), key=lambda x: x[0]):
        print(f"  q={q}: {count}")


def train_model(df: pd.DataFrame) -> DecisionTreeClassifier:
    X = df[FEATURE_COLUMNS]
    y = df[LABEL_COLUMN]

    model = DecisionTreeClassifier(max_depth=4, random_state=42)
    model.fit(X, y)
    return model


def maybe_report_holdout_accuracy(df: pd.DataFrame) -> None:
 
    y = df[LABEL_COLUMN]
    if len(df) < 6 or y.nunique() < 2:
        print("Not enough diverse training data for a meaningful holdout accuracy estimate.")
        return

    X = df[FEATURE_COLUMNS]
    label_counts = y.value_counts()
    can_stratify = y.nunique() > 1 and label_counts.min() >= 2

    X_train, X_test, y_train, y_test = train_test_split(
        X,
        y,
        test_size=0.30,
        random_state=42,
        stratify=y if can_stratify else None,
    )

    model = DecisionTreeClassifier(max_depth=4, random_state=42)
    model.fit(X_train, y_train)
    preds = model.predict(X_test)
    acc = accuracy_score(y_test, preds)
    print(f"Estimated holdout accuracy: {acc:.3f}")


def predict_for_workload(model: DecisionTreeClassifier, workload_path: Path) -> None:
    features = parse_workload(workload_path)
    row = pd.DataFrame([features], columns=FEATURE_COLUMNS)
    pred = model.predict(row)[0]

    print(f"\nWorkload: {workload_path}")
    print("Computed features:")
    for key in FEATURE_COLUMNS:
        value = features[key]
        if key == "num_processes":
            print(f"  {key}: {int(value)}")
        else:
            print(f"  {key}: {value:.3f}")
    print(f"\nRecommended Quantum: {pred}")


def main(argv: List[str]) -> int:
    if len(argv) not in (2, 3):
        usage()
        return 1

    training_csv = Path(argv[1])
    if not training_csv.exists():
        print(f"ERROR: Training CSV not found: {training_csv}")
        return 1

    try:
        df = load_training_data(training_csv)
    except Exception as exc:
        print(f"ERROR: {exc}")
        return 1

    summarize_training_data(df)
    maybe_report_holdout_accuracy(df)

    model = train_model(df)

    if len(argv) == 2:
        most_common_q = Counter(df[LABEL_COLUMN].tolist()).most_common(1)[0][0]
        print("\nNo workload file supplied.")
        print("To predict for a new workload, run:")
        print("  python ml_rr_predict.py rr_training_data.csv new_tasks.txt")
        print(f"Most common quantum in the training set: {most_common_q}")
        return 0

    workload_path = Path(argv[2])
    if not workload_path.exists():
        print(f"ERROR: Workload file not found: {workload_path}")
        return 1

    try:
        predict_for_workload(model, workload_path)
    except Exception as exc:
        print(f"ERROR: {exc}")
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
