import os
import time
import argparse
import pandas as pd
import pyodbc

def benchmark_mssql_query(csv_path, sql_query, dsn, user, password, in_memory, use_index):
    df = pd.read_csv(csv_path)
    table_name = os.path.splitext(os.path.basename(csv_path))[0]
    index_col = df.columns[0]

    conn = pyodbc.connect(f"DSN={dsn};UID={user};PWD={password}")
    cursor = conn.cursor()

    cursor.execute(f"IF OBJECT_ID(N'{table_name}', N'U') IS NOT NULL DROP TABLE [{table_name}]")
    col_defs = [f"[{col}] NVARCHAR(MAX)" for col in df.columns]
    col_defs[0] = f"[{index_col}] NVARCHAR(MAX){' PRIMARY KEY' if use_index else ''}"

    if in_memory:
        create_stmt = (
            f"CREATE TABLE [{table_name}] ({', '.join(col_defs)}) "
            "WITH (MEMORY_OPTIMIZED = ON, DURABILITY = SCHEMA_ONLY);"
        )
    else:
        create_stmt = f"CREATE TABLE [{table_name}] ({', '.join(col_defs)});"

    cursor.execute(create_stmt)

    if not use_index:
        cursor.execute(f"CREATE INDEX idx_{index_col} ON [{table_name}] ([{index_col}]);")

    insert_stmt = f"INSERT INTO [{table_name}] VALUES ({', '.join(['?'] * len(df.columns))})"
    cursor.fast_executemany = True
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

    print(f"=== MSSQL Query Timing (ms) ===")
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
    parser.add_argument("--dsn", required=True)
    parser.add_argument("--user", required=True)
    parser.add_argument("--password", required=True)
    parser.add_argument("--in-memory", action="store_true")
    parser.add_argument("--use-index", action="store_true")
    args = parser.parse_args()
    benchmark_mssql_query(args.csv, args.query, args.dsn, args.user, args.password, args.in_memory, args.use_index)
