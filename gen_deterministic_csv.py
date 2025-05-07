import csv
import os

def ensure_dirs(path):
    os.makedirs(path, exist_ok=True)

def generate_deterministic_csv(filename, num_rows, column_types):
    with open(filename, 'w', newline='') as f:
        writer = csv.writer(f)
        header = [f"col_{i}" for i in range(len(column_types))]
        writer.writerow(header)

        for i in range(num_rows):
            row = []
            for j, col_type in enumerate(column_types):
                if col_type == 'int':
                    row.append(i + j)  # Ensures variation
                elif col_type == 'float':
                    row.append(round((i + j) * 1.111, 3))
                elif col_type == 'str':
                    row.append(f"val_{i}_{j}")
            writer.writerow(row)

    print(f"CSV written: {filename} ({num_rows} rows, {len(column_types)} cols)")

def main():
    base_dir = os.path.abspath(os.path.dirname(__file__))
    datasets_dir = os.path.join(base_dir, 'deterministic_datasets')

    test_cases = [
        {
            "name": "simple_3col",
            "num_rows": 1000,
            "column_types": ['int', 'float', 'str']
        },
        {
            "name": "medium_5col",
            "num_rows": 5000,
            "column_types": ['str', 'int', 'float', 'str', 'int']
        },
        {
            "name": "wide_10col",
            "num_rows": 2000,
            "column_types": ['int', 'float', 'str'] * 3 + ['int']
        },
        {
            "name": "long_1col",
            "num_rows": 100000,
            "column_types": ['int']
        },
        {
            "name": "xlong_2col",
            "num_rows": 1_000_000,
            "column_types": ['int', 'str']
        },
    ]

    for test in test_cases:
        name = test["name"]
        print(f"\nGenerating: {name}")
        ds_path = os.path.join(datasets_dir, name)
        ensure_dirs(ds_path)

        csv_path = os.path.join(ds_path, 'data.csv')
        generate_deterministic_csv(
            filename=csv_path,
            num_rows=test["num_rows"],
            column_types=test["column_types"]
        )

if __name__ == '__main__':
    main()
