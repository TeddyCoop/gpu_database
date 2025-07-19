import os
import time
import argparse
import subprocess

skip_database_creation = False

def run_gdb_query(csv_path, query_path):
    if not os.path.exists("gdb_logs"):
        os.mkdir("gdb_logs")

    if not os.path.exists(csv_path):
        print(f"Error: CSV file '{csv_path}' not found.")
        return

    if not os.path.exists(query_path):
        print(f"Error: Query file '{query_path}' not found.")
        return

    with open(query_path, 'r') as f:
        query = f.read().strip()

    # create the table
    table_name = os.path.basename(os.path.dirname(csv_path))
    if not os.path.exists(f"gdb_data/benchmark/{table_name}"):
        create_query = f"CREATE DATABASE benchmark; IMPORT INTO {table_name} FROM '{csv_path}'"
        cmd = ["gdb.exe", f'--query="{create_query}"']
        print("Running command:", " ".join(cmd))
        result = subprocess.run(cmd, capture_output=True, text=True)

        if os.path.exists(f"gdb_logs/{table_name}_create.txt"):
            os.remove(f"gdb_logs/{table_name}_create.txt")
        if os.path.exists(f"gdb_logs/{table_name}_create.json"):
            os.remove(f"gdb_logs/{table_name}_create.json")

        try:
            os.rename("log.txt", f"gdb_logs/{table_name}_create.txt")
            os.rename("profile.json", f"gdb_logs/{table_name}_create.json")
        except Exception as e:
            print("failed to rename log.txt and profile.json")

        print(result.stderr)
        if result.returncode != 0:
            print("Failed to run CREATE command... exiting")
            print(cmd)
            return

    cmd = ["gdb.exe", f'--query="USE benchmark; {query.replace("data", table_name)}"']
    print("Running command:", " ".join(cmd))
    start_time = time.perf_counter()
    result = subprocess.run(cmd, capture_output=True, text=True)
    end_time = time.perf_counter()
    print(result.stderr)
    if result.returncode != 0:
        print("Failed to run SELECT command... exiting")
        print(cmd)
        return

    total_time_ms = (end_time - start_time) * 1000

    #print("=== GDB Query Result ===")
    if os.path.exists(f"gdb_logs/{table_name}_select.txt"):
        os.remove(f"gdb_logs/{table_name}_select.txt")
    if os.path.exists(f"gdb_logs/{table_name}_select.json"):
        os.remove(f"gdb_logs/{table_name}_select.json")

    try:
        os.rename("log.txt", f"gdb_logs/{table_name}_select.txt")
        os.rename("profile.json", f"gdb_logs/{table_name}_select.json")
    except Exception as e:
        print("failed to rename log.txt and profile.json")
    #print(f"Total Time: {total_time_ms:.3f} ms")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Benchmark query execution using gdb.exe")
    parser.add_argument("csv", help="Path to CSV file")
    parser.add_argument("query", help="Path to SQL query file")

    args = parser.parse_args()
    run_gdb_query(args.csv, args.query)
