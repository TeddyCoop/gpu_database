import os
import time
import argparse
import pandas as pd
import psycopg2

def benchmark_postgres_query(csv_path, sql_query, host, user, password, database, in_memory, use_index):
    df = pd.read_csv(csv_path)
    table_name = os.path.splitext(os.path.basename(csv_path))[0]
    index_col = df.columns[0]

    conn = psycopg2.connect(host=host, user=user, password=password, dbname=database)
    cursor = conn.cursor()

    cursor.execute(f'DROP TABLE IF EXISTS "{table_name}";')
    col_defs = [f'"{col}" TEXT' for col in df.columns]
    col_defs[0] = f'"{index_col}" TEXT{" PRIMARY KEY" if use_index else ""}'
    create_prefix = "CREATE UNLOGGED TABLE" if in_memory else "CREATE TABLE"
    create_stmt = f'{create_prefix} "{table_name}" ({", ".join(col_defs)});'
    cursor.execute(create_stmt)

    if not use_index:
        cursor.execute(f'CREATE INDEX idx_{index_col} ON "{table_name}" ("{index_col}");')

    insert_stmt = f'INSERT INTO "{table_name}" VALUES ({", ".join(["%s"] * len(df.columns))})'
    cursor.executemany(insert_stmt, df.values.tolist())
    conn.commit()

    start_prepare = time.perf_counter()
    cursor.execute(sql_query)
    prepare_time_ms = (time.perf_counter() - start_prepare) * 1000

    start_exec = time.perf_counter()
    row_count = sum(1 for _ in cursor)
    exec_time_ms = (time.perf_counter() - start_exec) * 1000

    start_fetch = time.perf_counter()
    cursor.execute(sql_query)
    _ = cursor.fetchall()
    fetch_time_ms = (time.perf_counter() - start_fetch) * 1000

    total_time_ms = prepare_time_ms + exec_time_ms + fetch_time_ms

    print(f"=== PostgreSQL Query Timing (ms) ===")
    print(f"In-Memory Table     : {in_memory}")
    print(f"Indexed Column      : {use_index}")
    print(f"Query Prepare Time  : {prepare_time_ms:.3f}")
    print(f"Query Exec Time     : {exec_time_ms:.3f}")
    print(f"Fetch Time          : {fetch_time_ms:.3f}")
    print(f"Total Query Time    : {total_time_ms:.3f}")
    print(f"Result Rows         : {row_count}")

    conn.close()

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("csv")
    parser.add_argument("query")
    parser.add_argument("--host", default="localhost")
    parser.add_argument("--user", required=True)
    parser.add_argument("--password", required=True)
    parser.add_argument("--database", required=True)
    parser.add_argument("--in-memory", action="store_true")
    parser.add_argument("--use-index", action="store_true")
    args = parser.parse_args()
    benchmark_postgres_query(args.csv, args.query, args.host, args.user, args.password, args.database, args.in_memory, args.use_index)
