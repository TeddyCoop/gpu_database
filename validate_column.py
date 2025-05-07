import sys
import struct

def read_u64(f):
    return struct.unpack('<Q', f.read(8))[0]

def check_column_file(path):
    with open(path, 'rb') as f:
        # Read variable_capacity
        var_cap = read_u64(f)
        print(f"Variable capacity: {var_cap} bytes")

        # Skip to start of offsets
        f.seek(0, 2)
        file_size = f.tell()
        offset_base = 8 + var_cap
        offset_count = (file_size - offset_base) // 8

        print(f"Detected {offset_count} rows")
        f.seek(offset_base)

        offsets = []
        for i in range(offset_count):
            off = read_u64(f)
            offsets.append(off)

        # Validate offsets
        error_count = 0
        for i, end in enumerate(offsets):
            start = offsets[i - 1] if i > 0 else 0
            if end < start:
                error_count+=1
            #    print(f"[ERROR] Row {i}: end offset {end} < start offset {start}")
            #else:
                #print(f"[OK] Row {i}: start={start}, end={end}, length={end-start}")
        print(f"Error count: {error_count}")

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: python check_column_file.py <column_file>")
        sys.exit(1)
    check_column_file(sys.argv[1])

#python validate_column.py build\data\testing\xlong_2col\col_1.dat