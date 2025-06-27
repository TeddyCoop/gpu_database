import os
import time
import argparse
import sqlite3
import pandas as pd

def benchmark_sqlite_query(csv_path, query_path, use_index):
    if not os.path.exists(csv_path):
        print(f"Error: File '{csv_path}' does not exist.")
        return

    if not os.path.exists(query_path):
        print(f"Error: Query file '{query_path}' does not exist.")
        return

    with open(query_path, 'r') as f:
        sql_query = f.read().strip()

    df = pd.read_csv(csv_path)
    table_name = os.path.splitext(os.path.basename(csv_path))[0]
    index_col = df.columns[0]

    conn = sqlite3.connect(":memory:")
    cursor = conn.cursor()

    col_defs = [f'"{col}" TEXT' for col in df.columns]
    col_defs[0] = f'"{index_col}" TEXT PRIMARY KEY' if use_index else f'"{index_col}" TEXT'
    cursor.execute(f'CREATE TABLE IF NOT EXISTS "{table_name}" ({", ".join(col_defs)});')

    if not use_index:
        cursor.execute(f'CREATE INDEX idx_{index_col} ON "{table_name}" ("{index_col}");')

    insert_stmt = f'INSERT INTO "{table_name}" VALUES ({", ".join(["?"] * len(df.columns))})'
    cursor.executemany(insert_stmt, df.values.tolist())
    conn.commit()

    start_prepare = time.perf_counter()
    cursor.execute(sql_query)
    prepare_time_ms = (time.perf_counter() - start_prepare) * 1000

    start_exec = time.perf_counter()
    row_count = sum(1 for _ in cursor)
    exec_time_ms = (time.perf_counter() - start_exec) * 1000

    start_fetch = time.perf_counter()
    _ = cursor.fetchall()
    fetch_time_ms = (time.perf_counter() - start_fetch) * 1000

    total_time_ms = prepare_time_ms + exec_time_ms + fetch_time_ms

    print(f"=== SQLite Query Timing (ms) ===")
    print(f"Indexed Column     : {use_index}")
    print(f"Query Prepare Time : {prepare_time_ms:.3f}")
    print(f"Query Exec Time    : {exec_time_ms:.3f}")
    print(f"Fetch Time         : {fetch_time_ms:.3f}")
    print(f"Total Query Time   : {total_time_ms:.3f}")
    print(f"Result Rows        : {row_count}")

    conn.close()

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("csv")
    parser.add_argument("query")
    parser.add_argument("--use-index", action="store_true")
    args = parser.parse_args()
    benchmark_sqlite_query(args.csv, args.query, args.use_index)
