import os
import time
import argparse
import sqlite3
import pandas as pd
import tempfile

def benchmark_sqlite_query(csv_path, query_path, use_index, in_memory, chunk_size=50000):
    if not os.path.exists(csv_path) or not os.path.exists(query_path):
        print(f"[ERROR] Missing input files")
        return

    with open(query_path, 'r') as f:
        sql_query = f.read().strip().rstrip(';')

    db_path = ":memory:" if in_memory else tempfile.NamedTemporaryFile(delete=False, suffix=".sqlite").name
    connection = sqlite3.connect(db_path)
    cursor = connection.cursor()
    table_name = os.path.splitext(os.path.basename(csv_path))[0]

    # === Load CSV into SQLite ===
    load_time_begin = time.perf_counter()
    csv_iter = pd.read_csv(csv_path, chunksize=chunk_size)
    first_chunk = next(csv_iter)
    index_col = first_chunk.columns[0]

    col_defs = [f'"{col}" TEXT' for col in first_chunk.columns]
    if use_index:
        col_defs[0] = f'"{index_col}" TEXT PRIMARY KEY'

    cursor.execute(f'CREATE TABLE IF NOT EXISTS "{table_name}" ({", ".join(col_defs)});')
    insert_stmt = f'INSERT INTO "{table_name}" VALUES ({", ".join(["?"] * len(first_chunk.columns))})'
    cursor.executemany(insert_stmt, first_chunk.values.tolist())

    for chunk in csv_iter:
        cursor.executemany(insert_stmt, chunk.values.tolist())

    if not use_index:
        cursor.execute(f'CREATE INDEX IF NOT EXISTS idx_{index_col} ON "{table_name}" ("{index_col}");')

    connection.commit()
    load_time_total_ms = (time.perf_counter() - load_time_begin) * 1000

    # === Warm-up run (optional but helps for fair comparison) ===
    try:
        cursor.execute(sql_query)
        cursor.fetchall()
    except Exception as e:
        print(f"[ERROR] Query failed during warm-up: {e}")
        return

    # === Time the query execution ===
    try:
        exec_time_begin = time.perf_counter()
        cursor.execute(sql_query)
        _ = cursor.fetchall()  # You can comment this line if you want to exclude fetch time
        exec_time_total_ms = (time.perf_counter() - exec_time_begin) * 1000
    except Exception as e:
        print(f"[ERROR] Query failed: {e}")
        return

    print(f"SQLite Benchmark Results:")
    print(f"  Database location  : {'in-memory' if in_memory else db_path}")
    print(f"  Table name         : {table_name}")
    print(f"  Index on first col : {'Yes' if use_index else 'No'}")
    print(f"  Load time (ms)     : {load_time_total_ms:.2f}")
    print(f"  Query time (ms)    : {exec_time_total_ms:.2f}")

    # Clean up if temp file
    #if not in_memory:
        #os.remove(db_path)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("csv")
    parser.add_argument("query")
    parser.add_argument("--use-index", action="store_true")
    parser.add_argument("--in-memory", action="store_true")
    parser.add_argument("--chunk-size", type=int, default=50000, help="Number of rows per chunk to load from CSV")
    args = parser.parse_args()

    benchmark_sqlite_query(args.csv, args.query, args.use_index, args.in_memory, args.chunk_size)
