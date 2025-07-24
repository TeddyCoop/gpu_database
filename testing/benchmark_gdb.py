import os
import time
import argparse
import subprocess
import re

def parse_time_from_log(log_path, label, unit='ms'):
    if not os.path.exists(log_path):
        return None

    with open(log_path, 'r') as f:
        for line in f:
            if label in line:
                # Find the number AFTER the label
                label_index = line.find(label)
                after_label = line[label_index + len(label):]
                match = re.search(r'(\d+(\.\d+)?)', after_label)
                if match:
                    val = float(match.group(1))
                    if unit == 'ms':
                        if 'micro' in after_label:
                            val /= 1000.0
                        elif 'ms' in after_label:
                            pass
                        elif 's' in after_label:
                            val *= 1000.0
                    return val
    return None


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

    # Step 1: Create the table
    table_name = os.path.basename(os.path.dirname(csv_path))
    db_created = os.path.exists(f"gdb_data/benchmark/{table_name}")  # Simple check

    if not db_created:
        create_query = f"CREATE DATABASE benchmark; IMPORT INTO {table_name} FROM '{csv_path}'"
        cmd = ["gdb.exe", f'--query="{create_query}"']
        result = subprocess.run(cmd, capture_output=True, text=True)

        try:
            os.rename("log.txt", f"gdb_logs/{table_name}_create.txt")
            os.rename("profile.json", f"gdb_logs/{table_name}_create.json")
        except Exception:
            pass  # Fail silently if rename fails

        if result.returncode != 0:
            print("[ERROR] Failed to run CREATE command")
            return

    # Step 2: Run SELECT query
    cmd = ["gdb.exe", f'--query="USE benchmark; {query.replace("data", table_name)}"']
    result = subprocess.run(cmd, capture_output=True, text=True)

    try:
        os.rename("log.txt", f"gdb_logs/{table_name}_select.txt")
        os.rename("profile.json", f"gdb_logs/{table_name}_select.json")
    except Exception:
        pass

    if result.returncode != 0:
        print("[ERROR] Failed to run SELECT command")
        return

    # Step 3: Parse output times
    load_time_ms = parse_time_from_log(f"gdb_logs/{table_name}_create.txt", "load from disk total time")
    query_time_ms = parse_time_from_log(f"gdb_logs/{table_name}_select.txt", "total 'SELECT' query time")
    gpu_kernel_time_ms = parse_time_from_log(f"gdb_logs/{table_name}_select.txt", "gpu kernel total execution")

    # Step 4: Output like SQLite
    print(f"GDB Benchmark Results:")
    print(f"  Database location  : gdb_data/benchmark/{table_name}")
    print(f"  Table name         : {table_name}")
    print(f"  Index on first col : N/A")
    print(f"  Load time (ms)     : {load_time_ms:.3f}" if load_time_ms is not None else "  Load time (ms)     : 0")
    print(f"  Query time (ms)    : {query_time_ms:.3f}" if query_time_ms is not None else "  Query time (ms)    : N/A")
    print(f"  Kernel time (ms)   : {gpu_kernel_time_ms:.3f}" if gpu_kernel_time_ms is not None else "  Kernel Time (ms)    : N/A")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Benchmark query execution using gdb.exe")
    parser.add_argument("csv", help="Path to CSV file")
    parser.add_argument("query", help="Path to SQL query file")

    args = parser.parse_args()
    run_gdb_query(args.csv, args.query)
