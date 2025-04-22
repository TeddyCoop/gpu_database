import csv
import random
import string

def random_string(length):
    return ''.join(random.choices(string.ascii_letters + string.digits, k=length))

def generate_csv(filename, num_rows, column_types, string_length=10):
    with open(filename, 'w', newline='') as f:
        writer = csv.writer(f)

        header = [f"col_{i}_{col_type}" for i, col_type in enumerate(column_types)]
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
                else:
                    raise ValueError(f"Unknown column type: {col_type}")
            writer.writerow(row)
            if i % 100000 == 0:
                print(f"Wrote {i:,} rows...")

column_types = ['str', 'int', 'float', 'str', 'float', 'int', 'str', 'str', 'float', 'int']

generate_csv(
    filename='typed_test_db.csv',
    num_rows=10_000_000,
    column_types=column_types,
    string_length=36
)
