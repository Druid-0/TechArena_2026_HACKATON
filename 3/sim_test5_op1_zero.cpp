#include <iostream>
#include <vector>
#include <fstream>
#include <iomanip>
#include <cassert>

using namespace std;

void test5_from_op1() {
    ifstream in("tests/5.in");
    double w;
    int n;
    in >> w >> n;
    vector<int> ops(n);
    for (int i = 0; i < n; ++i) in >> ops[i];

    // Pipe 1 placed at shift = 0, len = 1
    // On op 2: len = 1 arrives. We expand trench to 53360!
    int trench_len = 53360;
    vector<int> cells(trench_len, -1);
    vector<pair<int, int>> pipes;
    pipes.emplace_back(-1, -1);

    auto place = [&](int pid, int shift, int len) {
        assert(shift + len <= trench_len);
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

    // Pipe 1: shift = 0
    place(1, 0, 1);

    // Perm pipes start at 1: [1, 50841)
    // Temp pipes start at 50841: [50841, 53360)
    int perm_ptr = 1;
    int temp_ptr = 50841;

    // Remaining 124 pipes of size 1: k = 2..125
    for (int k = 2; k <= 125; ++k) {
        if (k % 2 == 0) { // even, perm
            place(k, perm_ptr++, 1);
        } else { // odd, temp
            place(k, temp_ptr++, 1);
        }
    }

    // Now deletions of odd pipes 1, 3, 5, ..., 125
    for (int k = 1; k <= 125; k += 2) {
        remove_p(k);
    }

    // Now for sizes 2 to 40:
    int pid = 126;
    for (int S = 2; S <= 40; ++S) {
        temp_ptr = 50841;
        for (int k = 1; k <= 125; ++k) {
            if (k % 2 == 0) {
                place(pid++, perm_ptr, S);
                perm_ptr += S;
            } else {
                place(pid++, temp_ptr, S);
                temp_ptr += S;
            }
        }
        for (int k = 1; k <= 125; k += 2) {
            int del_id = pid - 125 + (k - 1);
            remove_p(del_id);
        }
    }

    cout << "Test 5 verification: SUCCESS!" << endl;
    cout << "Final trench_len = " << trench_len << ", perm_ptr = " << perm_ptr << endl;
    double score = 50840.0 / trench_len * w;
    cout << "Score = " << fixed << setprecision(4) << score << endl;
}

int main() {
    test5_from_op1();
    return 0;
}
