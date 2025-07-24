import os
import subprocess
import argparse
import time
import sys
import csv

benchmark_results = []  # global list to store results


# Define engines and their corresponding runner script filenames
ENGINE_SCRIPTS = {
    "sqlite": "benchmark_sqlite.py",
    "duckdb": "benchmark_duckdb.py",
    "gdb": "benchmark_gdb.py",
}

def run_benchmark(engine, csv_path, query_path, args):
    script = ENGINE_SCRIPTS.get(engine)
    if not script:
        print(f"[WARN] No benchmark script defined for engine: {engine}")
        return

    cmd = [sys.executable, script, csv_path, query_path]
    if engine in ["sqlite", "duckdb"] and args.in_memory:
        cmd.append("--in-memory")
    if args.use_index and engine != "gdb":
        cmd.append("--use-index")

    dataset_name = os.path.basename(os.path.dirname(csv_path))

    print(f"\n>>> Running benchmark for {engine.upper()} on {dataset_name}")
    start = time.time()
    result = subprocess.run(cmd, capture_output=True, text=True)
    end = time.time()

    # Extract load time and query time from stdout
    load_time, query_time = None, None
    for line in result.stdout.splitlines():
        if "Load time" in line:
            load_time = float(line.split(":")[-1].strip().split()[0])
        elif "Query time" in line:
            query_time = float(line.split(":")[-1].strip().split()[0])

    benchmark_results.append({
        "dataset": dataset_name,
        "engine": engine,
        "load_time_ms": f"{load_time:.2f}" if load_time is not None else "N/A",
        "query_time_ms": f"{query_time:.2f}" if query_time is not None else "N/A",
        "duration_total_s": f"{end - start:.2f}"
    })

    print(f"--- Finished {engine.upper()} in {end - start:.2f}s ---")


def main():
    parser = argparse.ArgumentParser(description="Run all DB benchmark scripts on generated test cases.")
    parser.add_argument("--dataset-dir", default="datasets", help="Root folder containing datasets")
    parser.add_argument("--queries-dir", default="queries", help="Root folder containing query files")
    parser.add_argument("--in-memory", action="store_true", help="Use in-memory tables if supported")
    parser.add_argument("--use-index", action="store_true", help="Enable indexing on primary column")

    parser.add_argument("--engines", nargs="*", default=list(ENGINE_SCRIPTS.keys()),
                        help="Subset of engines to run (default: all)")
    parser.add_argument("--blacklist", nargs="*", default=[], help="List of dataset names to skip")
    parser.add_argument("--whitelist", nargs="*", default=[], help="Only run benchmarks for these dataset names")

    args = parser.parse_args()

    dataset_root = os.path.abspath(args.dataset_dir)
    query_root = os.path.abspath(args.queries_dir)

    args.engines = ["gdb", "duckdb", "sqlite"]
    args.in_memory = False
    args.use_index = False
    #args.blacklist = [""]
    #args.whitelist = ["long_string_3col"]
    args.blacklist = ["indexed_one_billion_rows_2col", "hundred_million_rows_2col"]

    for test_name in os.listdir(dataset_root):
        if args.whitelist and test_name not in args.whitelist:
            print(f"[SKIP] Not in whitelist: {test_name}")
            continue

        if test_name in args.blacklist:
            print(f"[SKIP] Blacklisted dataset: {test_name}")
            continue
        
        csv_path = os.path.join(dataset_root, test_name, "data.csv")
        for engine in args.engines:
            query_path = os.path.join(query_root, test_name, f"query_0.{engine}.sql")
            if os.path.exists(csv_path) and os.path.exists(query_path):
                run_benchmark(engine, csv_path, query_path, args)
            else:
                print(f"[SKIP] Missing file for {test_name}: {engine}")

    # === Write benchmark summary to CSV ===
    summary_path = "benchmark_logs/summary.csv"
    with open(summary_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=[
            "dataset", "engine", "load_time_ms", "query_time_ms", "duration_total_s"
        ])
        writer.writeheader()
        writer.writerows(benchmark_results)

    print(f"\nSummary written to {summary_path}")


if __name__ == "__main__":
    main()