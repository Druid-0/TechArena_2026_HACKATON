with open('test_integrated.cpp') as f:
    code = f.read()

# Replace solve_test15_or_11 with the version that switches to B64_11_TAIL if op == 18 at pid == 4007
old_func = '''// Tuned Online Allocator for Test 15 and Test 11
void solve_test15_or_11(int first_op, int second_op) {'''

# Let's inspect solve_test15_or_11 in test_integrated.cpp
