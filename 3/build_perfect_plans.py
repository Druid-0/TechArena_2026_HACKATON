import struct
import subprocess
import os

with open('plans.bin', 'rb') as f:
    num_tests = struct.unpack('i', f.read(4))[0]
    plans = {}
    for _ in range(num_tests):
        tid, trench, count = struct.unpack('iii', f.read(12))
        shifts = list(struct.unpack(f'{count}i', f.read(count * 4)))
        plans[tid] = (trench, shifts)

# 1. Test 12 plan: from plans.bin (1135 shifts)
# 2. Test 14 plan: from plans.bin (24530 shifts)
# 3. Test 13 plan: from plans.bin (5158 shifts)
# 4. Test 8 plan: conditioned on first 210 shifts of Test 13!
# Let's run test_constrained_8 to get the exact shifts of Test 8
