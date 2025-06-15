import sqlite3
import pandas as pd
import time
import argparse
import os

""" def benchmark_sql_query(csv_path, sql_query):
    if not os.path.exists(csv_path):
        print(f"Error: File '{csv_path}' does not exist.")
        return

    # Load CSV into DataFrame (not timed)
    df = pd.read_csv(csv_path)

    # Create SQLite in-memory DB and load table (not timed)
    conn = sqlite3.connect(":memory:")
    table_name = os.path.splitext(os.path.basename(csv_path))[0]
    df.to_sql(table_name, conn, index=False, if_exists="replace")

    cursor = conn.cursor()

    # Time just the query execution (compile + run)
    start_query = time.perf_counter()
    cursor.execute(sql_query)
    exec_time_ms = (time.perf_counter() - start_query) * 1000

    # Time just fetching the results
    start_fetch = time.perf_counter()
    results = cursor.fetchall()
    fetch_time_ms = (time.perf_counter() - start_fetch) * 1000

    total_time_ms = exec_time_ms + fetch_time_ms

    print(f"=== Query Timing (ms) ===")
    print(f"CSV File         : {csv_path}")
    print(f"Table Name       : {table_name}")
    print(f"Row Count        : {len(df)}")
    print(f"Column Count     : {len(df.columns)}")
    print(f"Query Exec Time  : {exec_time_ms:.3f} ms")
    print(f"Result Fetch Time: {fetch_time_ms:.3f} ms")
    print(f"Total Query Time : {total_time_ms:.3f} ms")
    print(f"Result Rows      : {len(results)}")

    conn.close() """

def benchmark_sqlite_query(csv_path, sql_query):
    if not os.path.exists(csv_path):
        print(f"Error: File '{csv_path}' does not exist.")
        return

    # Load CSV into DataFrame (not timed)
    df = pd.read_csv(csv_path)

    # Create SQLite in-memory DB and load table (not timed)
    conn = sqlite3.connect(":memory:")
    table_name = os.path.splitext(os.path.basename(csv_path))[0]
    df.to_sql(table_name, conn, index=False, if_exists="replace")

    cursor = conn.cursor()

    # Phase 1: Compile/prepare (matches clCreateKernel + clSetKernelArgs)
    start_prepare = time.perf_counter()
    cursor.execute(sql_query)
    prepare_time_ms = (time.perf_counter() - start_prepare) * 1000

    # Phase 2: Execution (matches clEnqueueNDRangeKernel + clWaitForEvents)
    start_exec = time.perf_counter()
    row_count = 0
    for _ in cursor:
        row_count += 1
    exec_time_ms = (time.perf_counter() - start_exec) * 1000

    # Phase 3: Optional fetch (matches clEnqueueReadBuffer)
    start_fetch = time.perf_counter()
    cursor.execute(sql_query)
    results = cursor.fetchall()
    fetch_time_ms = (time.perf_counter() - start_fetch) * 1000

    total_time_ms = prepare_time_ms + exec_time_ms + fetch_time_ms

    print(f"=== SQLite Query Timing (ms) ===")
    print(f"CSV File           : {csv_path}")
    print(f"Table Name         : {table_name}")
    print(f"Row Count          : {len(df)}")
    print(f"Column Count       : {len(df.columns)}")
    print(f"Query Prepare Time : {prepare_time_ms:.3f} ms")
    print(f"Query Exec Time    : {exec_time_ms:.3f} ms")   # use this to compare to GPU kernel exec
    print(f"Result Fetch Time  : {fetch_time_ms:.3f} ms")
    print(f"Total Query Time   : {total_time_ms:.3f} ms")
    print(f"Result Rows        : {row_count}")

    conn.close()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Time SQL query execution and fetch (ms) against in-memory SQLite from a CSV file.")
    parser.add_argument("csv", help="Path to CSV file")
    parser.add_argument("query", help="SQL query to run on the CSV data")

    args = parser.parse_args()
    benchmark_sqlite_query(args.csv, args.query)

# python benchmark_sqlite.py build/data/massive_2col.csv "SELECT * FROM massive_2col WHERE (col_1 >= 4070.46 AND col_1 <= 4371.73 OR col_0 >= 197555) AND (col_0 >= 846962 OR col_1 >= 1012.13 AND col_1 <= 1036.33);"
# python benchmark_sqlite.py build/data/huge_100col.csv "SELECT * FROM huge_100col WHERE (col_51 == 'B9COt' OR col_82 >= 879.49 AND col_82 <= 1129.8) AND (col_23 >= 581547 OR col_42 == 'W8Xug' OR col_86 >= 573325)"
# python benchmark_sqlite.py build/data/triple_string_3col.csv "SELECT * FROM triple_string_3col WHERE (col_0 == 'LZ2KU' OR col_2 == 'iZaBC' OR col_1 CONTAINS 'IMt')"
# python benchmark_sqlite.py ../build/data/large_3col.csv "SELECT * FROM large_3col WHERE (col_0 == 7)"
