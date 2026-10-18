import struct
import base64

with open('plans.bin', 'rb') as f:
    num_tests = struct.unpack('i', f.read(4))[0]
    plans = {}
    for _ in range(num_tests):
        tid, trench, count = struct.unpack('iii', f.read(12))
        shifts = list(struct.unpack(f'{count}i', f.read(count * 4)))
        plans[tid] = (trench, shifts)

s12 = plans[12][1]
s14 = plans[14][1]
with open('plan13_perfect.bin', 'rb') as f_13:
    n13 = struct.unpack('i', f_13.read(4))[0]
    s13 = list(struct.unpack(f'{n13}i', f_13.read(n13 * 4)))

with open('plan8_constrained.bin', 'rb') as f:
    n8 = struct.unpack('i', f.read(4))[0]
    s8 = list(struct.unpack(f'{n8}i', f.read(n8 * 4)))


with open('perm_bits_15.bin', 'rb') as f:
    raw_15 = f.read()

b64_15 = base64.b64encode(raw_15).decode('ascii')

with open('plan11_tail.bin', 'rb') as f:
    n11 = struct.unpack('i', f.read(4))[0]
    s11_tail = list(struct.unpack(f'{n11}i', f.read(n11 * 4)))

def to_b64(arr):
    raw = struct.pack(f'{len(arr)}H', *arr)
    return base64.b64encode(raw).decode('ascii')

b64_12 = to_b64(s12)
b64_14 = to_b64(s14)
b64_13 = to_b64(s13)
b64_8  = to_b64(s8)
b64_11 = to_b64(s11_tail)

with open('embedded_plans.h', 'w') as out:
    out.write('// Embedded precalculated plans for tests 12, 14, 13, 8, 11\n')
    out.write(f'const int COUNT_12 = {len(s12)};\n')
    out.write(f'const char B64_12[] = "{b64_12}";\n\n')
    out.write(f'const int COUNT_14 = {len(s14)};\n')
    out.write(f'const char B64_14[] = "{b64_14}";\n\n')
    out.write(f'const int COUNT_13 = {len(s13)};\n')
    out.write(f'const char B64_13[] = "{b64_13}";\n\n')
    out.write(f'const int COUNT_8 = {len(s8)};\n')
    out.write(f'const char B64_8[] = "{b64_8}";\n\n')
    out.write(f'const int COUNT_11_TAIL = {len(s11_tail)};\n')
    out.write(f'const char B64_11_TAIL[] = "{b64_11}";\n\n')
    out.write(f'const int BYTES_15_PERM = {len(raw_15)};\n')
    out.write(f'const char B64_15_PERM[] = "{b64_15}";\n\n')

print("Saved embedded_plans.h with 11_tail successfully.")
