import os
import subprocess
import argparse
import time
import sys

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
    if engine in ["sqlite", "duckdb"]:
        if args.in_memory:
            cmd.append("--in-memory")

    if args.use_index and engine != "gdb":
        cmd.append("--use-index")

    print(f"\n>>> Running benchmark for {engine.upper()}:")
    #print("Command:", cmd)
    start = time.time()
    subprocess.run(cmd)
    end = time.time()
    print(f"--- Finished {engine.upper()} in {end - start:.2f}s ---\n")

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

    args.engines = ["duckdb", "sqlite", "gdb"]
    args.in_memory = False
    args.use_index = False
    #args.blacklist = [""]
    #args.whitelist = ["indexed_one_billion_rows_2col"]
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

if __name__ == "__main__":
    main()