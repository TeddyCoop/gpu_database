import sqlite3
import csv
import time
import sys
import os

def import_csv_to_sqlite(db_path, table_name, csv_path):
    import_start = time.time()
    
    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()

    with open(csv_path, newline='') as csvfile:
        reader = csv.reader(csvfile)
        headers = next(reader)
        columns = ', '.join([f'"{h}" TEXT' for h in headers])
        cursor.execute(f'CREATE TABLE IF NOT EXISTS {table_name} ({columns})')

        placeholders = ', '.join(['?'] * len(headers))
        cursor.executemany(
            f'INSERT INTO {table_name} VALUES ({placeholders})',
            reader
        )

    conn.commit()
    conn.close()

    import_end = time.time()
    print(f"Import Time: {import_end - import_start:.6f} sec")

def run_benchmark(db_path, sql_query):
    print(f"Running query on {db_path}...")

    start_time = time.time()

    conn = sqlite3.connect(db_path)
    cursor = conn.cursor()

    query_start = time.time()
    cursor.execute(sql_query)
    query_end = time.time()
    rows = cursor.fetchall()

    conn.close()

    end_time = time.time()

    print(f"Total Time: {end_time - start_time:.6f} sec")
    print(f"Query Time: {query_end - query_start:.6f} sec")
    print(f"Rows Returned: {len(rows)}")

if __name__ == "__main__":
    if len(sys.argv) < 4:
        print("Usage: python benchmark_sqlite.py <csv_path> <table_name> <sql_query>")
        sys.exit(1)

    csv_path = sys.argv[1]
    table_name = sys.argv[2]
    sql_query = " ".join(sys.argv[3:])

    db_path = "temp_benchmark.db"
    if os.path.exists(db_path):
        os.remove(db_path)

    print(f"Importing {csv_path} into SQLite...")
    import_csv_to_sqlite(db_path, table_name, csv_path)

    run_benchmark(db_path, sql_query)

    os.remove(db_path)

# python benchmark_sqlite.py build/data/test1.csv test1 "SELECT col_0_str FROM test1 WHERE col_1_int == 235483;"
# python benchmark_sqlite.py build/data/gen_dataset_1_8gb.csv test1 "SELECT col_0_str FROM test1 WHERE col_1_int == 606126;"