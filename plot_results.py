#!/usr/bin/env python3
"""
Optional helper: visualize results.csv produced by ./lbsim.

Usage:
    pip install pandas matplotlib
    python3 plot_results.py [results.csv]
"""
import sys
import pandas as pd
import matplotlib.pyplot as plt

path = sys.argv[1] if len(sys.argv) > 1 else "results.csv"
df = pd.read_csv(path)

fig, axes = plt.subplots(2, 2, figsize=(12, 8))

df.plot(x="strategy", y=["avg_latency_ms", "p95_latency_ms", "p99_latency_ms"],
        kind="bar", ax=axes[0, 0], title="Latency (ms)")
axes[0, 0].tick_params(axis='x', rotation=30)

df.plot(x="strategy", y="throughput_rps", kind="bar", ax=axes[0, 1],
        title="Throughput (req/s)", legend=False, color="seagreen")
axes[0, 1].tick_params(axis='x', rotation=30)

df.plot(x="strategy", y="fairness_index", kind="bar", ax=axes[1, 0],
        title="Load Fairness (Jain's index, 1.0 = perfectly even)",
        legend=False, color="orange")
axes[1, 0].set_ylim(0, 1.05)
axes[1, 0].tick_params(axis='x', rotation=30)

df.plot(x="strategy", y="avg_queue_delay_ms", kind="bar", ax=axes[1, 1],
        title="Average Queue Delay (ms)", legend=False, color="indianred")
axes[1, 1].tick_params(axis='x', rotation=30)

plt.tight_layout()
plt.savefig("comparison.png", dpi=150)
print("Saved comparison.png")
