#!/usr/bin/env -S uv run --script
# /// script
# dependencies = [
#     "matplotlib",
#     "pandas",
# ]
# ///

import sys
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


INPUT = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("results.csv")
COMPETITORS = [
    ("gmp", "GMP"),
    ("boost", "Boost.Multiprecision"),
]
COLORS = {
    "big": "tab:blue",
    "gmp": "tab:orange",
    "boost": "tab:green",
}


def plot_line(ax, sub, column, label, color):
    if column not in sub:
        return False

    points = sub[["bits", column]].dropna()
    if points.empty:
        return False

    ax.plot(points["bits"], points[column], marker="o", label=label, color=color)
    return True


df = pd.read_csv(INPUT, comment="#", na_values=["n/a", "N/A", ""])
required = {"limbs", "bits", "op", "big_ns"}
missing = sorted(required - set(df.columns))
if missing:
    raise SystemExit(f"{INPUT} is missing required columns: {', '.join(missing)}")

numeric_cols = [
    column
    for column in df.columns
    if column in {"limbs", "bits"} or column.endswith("_ns") or column.endswith("_x")
]
df[numeric_cols] = df[numeric_cols].apply(pd.to_numeric, errors="coerce")

for competitor, _label in COMPETITORS:
    ns_column = f"{competitor}_ns"
    x_column = f"{competitor}_x"
    speedup_column = f"{competitor}_speedup_vs_big"

    if ns_column in df:
        df[speedup_column] = df["big_ns"] / df[ns_column]
        if x_column in df:
            df[speedup_column] = df[speedup_column].fillna(df[x_column])
    elif x_column in df:
        df[speedup_column] = df[x_column]

ops = list(df["op"].dropna().drop_duplicates())

# absolute timings
for op in ops:
    sub = df[df["op"] == op].sort_values(["bits", "limbs"])
    op_name = op

    fig, ax = plt.subplots(figsize=(9, 5.2))
    plotted = plot_line(ax, sub, "big_ns", "big", COLORS["big"])
    for competitor, label in COMPETITORS:
        plotted = plot_line(ax, sub, f"{competitor}_ns", label, COLORS[competitor]) or plotted

    if not plotted:
        plt.close(fig)
        continue

    ax.set_title(f"{str(op).upper()} performance")
    ax.set_xlabel("Bit width")
    ax.set_ylabel("ns/op, lower is better")
    ax.grid(True, alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(f"plot_{op_name}_ns.png", dpi=180)
    plt.close(fig)

# speedup ratios
for op in ops:
    sub = df[df["op"] == op].sort_values(["bits", "limbs"])
    op_name = op

    fig, ax = plt.subplots(figsize=(9, 5.2))
    plotted = False
    for competitor, label in COMPETITORS:
        plotted = plot_line(
            ax,
            sub,
            f"{competitor}_speedup_vs_big",
            f"{label} speedup",
            COLORS[competitor],
        ) or plotted

    if not plotted:
        plt.close(fig)
        continue

    ax.axhline(1.0, linewidth=1, linestyle="--", label="parity with big")
    ax.set_title(f"{str(op).upper()} speedup vs big")
    ax.set_xlabel("Bit width")
    ax.set_ylabel("Speedup, >1 means faster than big")
    ax.grid(True, alpha=0.3)
    ax.legend()
    fig.tight_layout()
    fig.savefig(f"plot_{op_name}_relative.png", dpi=180)
    plt.close(fig)

# quick final summary
summary = []
for op in ops:
    sub = df[df["op"] == op].sort_values(["bits", "limbs"])
    for competitor, _label in COMPETITORS:
        ns_column = f"{competitor}_ns"
        if ns_column not in sub:
            continue

        comparable = sub[["bits", "limbs", "big_ns", ns_column]].dropna()
        if comparable.empty:
            summary.append((op, competitor, "no comparable data"))
            continue

        faster = comparable[comparable[ns_column] < comparable["big_ns"]]
        if faster.empty:
            summary.append((op, competitor, "big faster for all measured sizes"))
        else:
            first = faster.iloc[0]
            summary.append((
                op,
                competitor,
                f"{competitor} first faster at {int(first.bits)} bits ({int(first.limbs)} limbs)",
            ))

print(pd.DataFrame(summary, columns=["op", "competitor", "summary"]))
