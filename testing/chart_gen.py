import pandas as pd
import matplotlib.pyplot as plt
import os
import numpy as np

# === CONFIG ===
summary_csv_path = "benchmark_logs/summary.csv"
latex_output_path = "benchmark_logs/benchmark_table.tex"
chart_output_path = "benchmark_logs/query_time_chart.pdf"

# === LOAD DATA ===
df = pd.read_csv(summary_csv_path)
# === Pivot for bar chart ===
pivot_df = df.pivot(index="dataset", columns="engine", values="query_time_ms")
pivot_df = pivot_df.sort_index()

# === Plot as grouped bar chart ===
engines = pivot_df.columns.tolist()
datasets = pivot_df.index.tolist()
x = np.arange(len(datasets))  # label locations
width = 0.2  # width of each bar (adjust based on # engines)

plt.figure(figsize=(14, 8))

# Draw bars for each engine
for i, engine in enumerate(engines):
    offset = (i - len(engines) / 2) * width + width / 2
    plt.bar(x + offset, pivot_df[engine], width=width, label=engine)

# === Axis formatting ===
plt.xticks(x, datasets, rotation=60, ha="right", fontsize=9)
plt.ylabel("Query Time (ms)", fontsize=12)
plt.title("Query Execution Time by Engine and Dataset", fontsize=14)
plt.legend(title="Engine", fontsize=10)
plt.tight_layout()

# === Save chart ===
plt.savefig("benchmark_logs/query_time_bar_chart.pdf")
plt.close()
print("Saved bar chart to benchmark_logs/query_time_bar_chart.pdf")
