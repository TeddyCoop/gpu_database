import matplotlib.pyplot as plt

# Data preparation
data = [
    {"dataset": "1.8 GB, 10M rows", "python_sqlite": 1.786279, "gdb_cold": 743.538, "gdb_cached": 560.669},
    {"dataset": "7.5 GB, 5M rows", "python_sqlite": 8.907471, "gdb_cold": 1035.385, "gdb_cached": 854.394},
    {"dataset": "3.3 MB, 10K rows", "python_sqlite": 0.026196, "gdb_cold": 726.271, "gdb_cached": 520.275},
]

# Convert seconds to milliseconds for consistency
def convert_to_ms(time_sec):
    return time_sec * 1000

# Prepare the data for plotting
datasets = [entry["dataset"] for entry in data]
python_sqlite_times = [convert_to_ms(entry["python_sqlite"]) for entry in data]
gdb_cold_times = [entry["gdb_cold"] for entry in data]
gdb_cached_times = [entry["gdb_cached"] for entry in data]

# Plotting
fig, ax = plt.subplots(figsize=(10, 6))

bar_width = 0.25  # Adjust bar width for spacing
x_positions = range(len(datasets))

# Plot bars for each category, adjusting x positions to avoid overlap
ax.bar([pos - bar_width for pos in x_positions], python_sqlite_times, width=bar_width, label="Python SQLite (warm)")
ax.bar(x_positions, gdb_cold_times, width=bar_width, label="GDB (cold)")
ax.bar([pos + bar_width for pos in x_positions], gdb_cached_times, width=bar_width, label="GDB (cold, cached)")

# Labels and title
ax.set_xlabel('Dataset')
ax.set_ylabel('Total Time (ms)')
ax.set_title('Total Query Execution Times for Different Datasets')
ax.legend()

# Rotate dataset names for better visibility
plt.xticks(x_positions, datasets, rotation=45)

# Show the plot
plt.tight_layout()
plt.show()
