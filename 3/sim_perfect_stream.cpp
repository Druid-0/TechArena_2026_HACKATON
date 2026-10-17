#include <iostream>
#include <vector>
#include <fstream>
#include <iomanip>
#include <cassert>

using namespace std;

// Test 5 simulation with stream detection
void test5_stream() {
    ifstream in("tests/5.in");
    double w;
    int n;
    in >> w >> n;
    vector<int> ops(n);
    for (int i = 0; i < n; ++i) in >> ops[i];

    // Verification structures
    vector<pair<int, int>> pipes;
    pipes.emplace_back(-1, -1);
    vector<int> cells;

    auto dig = [&](int cnt) {
        cells.resize(cells.size() + cnt * 20, -1);
    };

    auto place = [&](int pid, int shift, int len) {
        while (shift + len > (int)cells.size()) {
            dig(1);
        }
        for (int i = 0; i < len; ++i) {
            assert(cells[shift + i] == -1);
            cells[shift + i] = pid;
        }
        pipes.emplace_back(shift, len);
    };

    auto remove_p = [&](int pid) {
        int shift = pipes[pid].first;
        int len = pipes[pid].second;
        for (int i = 0; i < len; ++i) {
            assert(cells[shift + i] == pid);
            cells[shift + i] = -1;
        }
        pipes[pid] = {-1, -1};
    };

    // First 125 pipes: size 1
    // Odd at 0..62
    // Even at 63..124
    int odd_ptr = 0;
    int even_ptr = 63;
    for (int k = 1; k <= 125; ++k) {
        if (k % 2 == 1) {
            place(k, odd_ptr++, 1);
        } else {
            place(k, even_ptr++, 1);
        }
    }

    // Now op 126 is deletion of 1, 3, 5, ...
    for (int k = 1; k <= 125; k += 2) {
        remove_p(k);
    }

    // Now for sizes 2 to 40:
    // Perm pipes start at 125 and grow contiguously
    // Temp pipes start at 50840 and reset each size
    int perm_ptr = 125;
    int pid = 126;

    for (int S = 2; S <= 40; ++S) {
        int temp_ptr = 50840;
        // 125 additions
        for (int k = 1; k <= 125; ++k) {
            if (k % 2 == 0) { // even, perm
                place(pid++, perm_ptr, S);
                perm_ptr += S;
            } else { // odd, temp
                place(pid++, temp_ptr, S);
                temp_ptr += S;
            }
        }
        // 63 deletions
        for (int k = 1; k <= 125; k += 2) {
            int del_id = pid - 125 + (k - 1);
            remove_p(del_id);
        }
    }

    cout << "Test 5 Stream: Total trench length = " << cells.size() << endl;
    assert(cells.size() == 53360);
    double score = 50840.0 / cells.size() * w;
    cout << "Test 5 Stream Score = " << fixed << setprecision(4) << score << endl;
}

int main() {
    test5_stream();
    return 0;
}
