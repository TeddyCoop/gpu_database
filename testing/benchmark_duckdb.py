import duckdb
import time
import argparse
import os

def benchmark_duckdb_query(csv_path, query_path, use_index, in_memory):
    if not os.path.exists(csv_path):
        print(f"[ERROR] File '{csv_path}' does not exist.")
        return

    if not os.path.exists(query_path):
        print(f"[ERROR] Query file '{query_path}' does not exist.")
        return

    with open(query_path, 'r') as f:
        sql_query = f.read().strip().rstrip(';')

    db_path = ":memory:" if in_memory else f"{csv_path}.duckdb"
    con = duckdb.connect(database=db_path)
    table_name = os.path.splitext(os.path.basename(csv_path))[0]

    # === Step 1: Load CSV into DuckDB table ===
    print(f"[INFO] Loading CSV: {csv_path}")
    start_load = time.perf_counter()
    con.execute(f"""
        CREATE OR REPLACE TABLE {table_name} AS 
        SELECT * FROM read_csv_auto('{csv_path}', SAMPLE_SIZE=-1, AUTO_DETECT=TRUE);
    """)
    load_time_ms = (time.perf_counter() - start_load) * 1000

    # === Step 2: Index note (informational only) ===
    if use_index:
        print("[INFO] DuckDB does not support user-defined indexes - skipping.")

    # === Step 3: Warm-up run to avoid JIT/compilation effects ===
    try:
        con.execute(sql_query).fetchall()
    except Exception as e:
        print(f"[ERROR] Query failed during warm-up: {e}")
        return

    # === Step 4: Measure query execution time ===
    try:
        start_query = time.perf_counter()
        con.execute(sql_query).fetchall()  # You can comment out fetchall() to exclude result materialization
        exec_time_ms = (time.perf_counter() - start_query) * 1000
    except Exception as e:
        print(f"[ERROR] Query failed: {e}")
        return

    # === Output ===
    print(f"DuckDB Benchmark Results:")
    print(f"  Database location  : {'in-memory' if in_memory else db_path}")
    print(f"  Table name         : {table_name}")
    print(f"  Index on first col : N/A (not supported)")
    print(f"  Load time (ms)     : {load_time_ms:.2f}")
    print(f"  Query time (ms)    : {exec_time_ms:.2f}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("csv")
    parser.add_argument("query")
    parser.add_argument("--use-index", action="store_true")
    parser.add_argument("--in-memory", action="store_true")
    args = parser.parse_args()

    benchmark_duckdb_query(args.csv, args.query, args.use_index, args.in_memory)
