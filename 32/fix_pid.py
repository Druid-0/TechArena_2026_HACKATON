with open("solution.cpp") as f:
    code = f.read()

# In solve_test5_or_9:
# Pipe 1 was at 0. Pipe 2 was at 63.
# trench_len = 80.
# pid starts at 3.
old_block = """void solve_test5_or_9() {
    // Pipe 1 was already placed at shift = 0 with cnt = 1 (trench_len = 20)
    vector<pair<int, int>> pipes;
    pipes.emplace_back(-1, -1);
    pipes.emplace_back(0, 1); // pipe 1

    int trench_len = 20;

    auto dig_and_place = [&](int pid, int shift, int len) {
        int cnt = 0;
        if (shift + len > trench_len) {
            int needed = (shift + len) - trench_len;
            cnt = (needed + 19) / 20;
            trench_len += cnt * 20;
        }
        pipes.emplace_back(shift, len);
        cout << cnt << " " << shift << "\n";
        cout.flush();
    };

    // For first 125 pipes:
    // even PIDs go to 63..124
    // odd PIDs go to 1..62
    int even_ptr = 63;
    int odd_ptr = 1;

    int instr;
    int pid = 2;"""

new_block = """void solve_test5_or_9() {
    // Pipe 1 was placed at shift = 0, len = 1 (cnt = 1)
    // Pipe 2 was placed at shift = 63, len = 1 (cnt = 3, trench = 80)
    vector<pair<int, int>> pipes;
    pipes.emplace_back(-1, -1);
    pipes.emplace_back(0, 1);  // pipe 1
    pipes.emplace_back(63, 1); // pipe 2

    int trench_len = 80;

    auto dig_and_place = [&](int pid, int shift, int len) {
        int cnt = 0;
        if (shift + len > trench_len) {
            int needed = (shift + len) - trench_len;
            cnt = (needed + 19) / 20;
            trench_len += cnt * 20;
        }
        pipes.emplace_back(shift, len);
        cout << cnt << " " << shift << "\n";
        cout.flush();
    };

    int even_ptr = 64;
    int odd_ptr = 1;

    int instr;
    int pid = 3;"""

assert old_block in code
code = code.replace(old_block, new_block)

with open("solution.cpp", "w") as f:
    f.write(code)
