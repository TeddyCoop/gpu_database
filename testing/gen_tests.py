import csv
import os
import random
import string

def random_string(length):
    return ''.join(random.choices(string.ascii_letters + string.digits, k=length))

def generate_csv(filename, num_rows, column_types, string_length=10):
    with open(filename, 'w', newline='') as f:
        writer = csv.writer(f)
        header = [f"col_{i}" for i in range(len(column_types))]
        writer.writerow(header)

        for i in range(num_rows):
            row = []
            for col_type in column_types:
                if col_type == 'str':
                    row.append(random_string(string_length))
                elif col_type == 'int':
                    row.append(str(random.randint(0, 1_000_000)))
                elif col_type == 'float':
                    row.append(str(round(random.uniform(0, 10000), 2)))
            writer.writerow(row)
            if i % 100000 == 0 and i > 0:
                print(f"Wrote {i:,} rows to {filename}...")

def build_condition_tree(column_types):
    col_count = len(column_types)
    and_group_count = random.randint(1, 3)
    condition_tree = []

    for _ in range(and_group_count):
        group_size = random.randint(1, 4)
        chosen = random.sample(range(col_count), k=min(group_size, col_count))
        group = []
        for i in chosen:
            col_name = f"col_{i}"
            col_type = column_types[i]
            if col_type == 'int':
                val = random.randint(0, 1_000_000)
                group.append(("int", col_name, val))
            elif col_type == 'float':
                low = round(random.uniform(0, 5000), 2)
                high = round(low + random.uniform(0, 1000), 2)
                group.append(("float", col_name, low, high))
            elif col_type == 'str':
                substr = random_string(3)
                val = random_string(5)
                kind = random.choice(["contains", "equals"])
                group.append(("str", col_name, kind, substr if kind == "contains" else val))
        condition_tree.append(group)

    return condition_tree

def format_condition_tree(condition_tree, db):
    all_conds = []

    for group in condition_tree:
        conds = []
        for cond in group:
            if cond[0] == "int":
                _, col, val = cond
                conds.append(f"{col} >= {val}")
            elif cond[0] == "float":
                _, col, low, high = cond
                conds.append(f"{col} >= {low} AND {col} <= {high}")
            elif cond[0] == "str":
                _, col, kind, val = cond
                if db == 'sqlite':
                    if kind == "contains":
                        conds.append(f"{col} LIKE '%{val}%'")
                    else:
                        conds.append(f"{col} = '{val}'")
                elif db == 'postgres':
                    if kind == "contains":
                        conds.append(f"{col} ILIKE '%{val}%'")
                    else:
                        conds.append(f"{col} = '{val}'")
                else:
                    if kind == "contains":
                        conds.append(f"{col} CONTAINS '{val}'")
                    else:
                        conds.append(f"{col} == '{val}'")
        if len(conds) == 1:
            all_conds.append(conds[0])
        else:
            all_conds.append(f"({' OR '.join(conds)})")

    return ' AND '.join(all_conds)

def generate_query(column_types, db, condition_tree):
    where_clause = format_condition_tree(condition_tree, db)
    return f"SELECT * FROM data WHERE {where_clause};"

def ensure_dirs(path):
    os.makedirs(path, exist_ok=True)

test_cases = [
    {
        "name": "triple_string_3col",
        "num_rows": 10_000_000,
        "column_types": ['str', 'str', 'str'],
        "string_length": 16
    }
]

"""
    {
        "name": "medium_25col",
        "num_rows": 10000,
        "column_types": ['int', 'float', 'str'] * 8 + ['str'],
        "string_length": 24
    },
    {
        "name": "large_50col",
        "num_rows": 1000000,
        "column_types": ['str', 'int', 'float'] * 17,
        "string_length": 20
    },
    {
        "name": "huge_100col",
        "num_rows": 5000000,
        "column_types": ['str', 'float', 'int'] * 33 + ['str'],
        "string_length": 30
    },
    {
        "name": "bigger_5col",
        "num_rows": 20000000,
        "column_types": ['int', 'str', 'float', 'int', 'str'],
        "string_length": 36
    },
    {
        "name": "massive_2col",
        "num_rows": 100000000,
        "column_types": ['int', 'float'],
        "string_length": 16
    },
"""
def main():
    base_dir = os.path.abspath(os.path.dirname(__file__))
    datasets_dir = os.path.join(base_dir, 'datasets')
    queries_dir = os.path.join(base_dir, 'queries')

    for test in test_cases:
        name = test["name"]
        print(f"\nGenerating test case: {name}")

        ds_path = os.path.join(datasets_dir, name)
        q_path = os.path.join(queries_dir, name)
        ensure_dirs(ds_path)
        ensure_dirs(q_path)

        csv_path = os.path.join(ds_path, 'data.csv')
        generate_csv(
            filename=csv_path,
            num_rows=test["num_rows"],
            column_types=test["column_types"],
            string_length=test["string_length"]
        )

        condition_tree = build_condition_tree(test["column_types"])
        for i in range(5):
            sqlite_query = generate_query(test["column_types"], 'sqlite', condition_tree)
            # postgres_query = generate_query(test["column_types"], 'postgres', condition_tree)
            gdb_query = generate_query(test["column_types"], 'gdb', condition_tree)

            with open(os.path.join(q_path, f"query_{i}.sqlite.sql"), 'w') as f:
                f.write(sqlite_query + '\n')
            # with open(os.path.join(q_path, f"query_{i}.postgres.sql"), 'w') as f:
                # f.write(postgres_query + '\n')
            with open(os.path.join(q_path, f"query_{i}.gdb.sql"), 'w') as f:
                f.write(gdb_query + '\n')

        print(f"Completed: {name}")

if __name__ == '__main__':
    main()
