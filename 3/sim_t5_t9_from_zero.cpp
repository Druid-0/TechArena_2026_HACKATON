#include <iostream>
#include <vector>
#include <fstream>
#include <iomanip>
#include <cassert>

using namespace std;

double sim_test5() {
    ifstream in("tests/5.in");
    double w;
    int n;
    in >> w >> n;
    vector<int> ops(n);
    for (int i = 0; i < n; ++i) in >> ops[i];

    // Pipe 1 placed at shift = 0, len = 1
    // At op 2: len = 1 arrives. We expand trench to 53380.
    int trench_len = 53380;
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

    // Pipe 1 at 0
    place(1, 0, 1);

    // Perm pipes start at 1: [1, 50841)
    // Temp pipes start at 50841: [50841, 53361) <= 53380
    int perm_ptr = 1;
    int temp_ptr = 50841;

    for (int k = 2; k <= 125; ++k) {
        if (k % 2 == 0) {
            place(k, perm_ptr++, 1);
        } else {
            place(k, temp_ptr++, 1);
        }
    }

    for (int k = 1; k <= 125; k += 2) {
        remove_p(k);
    }

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

    int num_occ = 0;
    for (int c : cells) if (c > 0) num_occ++;
    double score = (double)num_occ / trench_len * w;
    cout << "Test 5 score: " << fixed << setprecision(4) << score << " (L=" << trench_len << ")" << endl;
    return score;
}

double sim_test9() {
    ifstream in("tests/9.in");
    double w;
    int n;
    in >> w >> n;
    vector<int> ops(n);
    for (int i = 0; i < n; ++i) in >> ops[i];

    int trench_len = 47520;
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

    place(1, 0, 1);

    // In test 9:
    // Perm pipes start at 1: [1, 31501)
    // Temp pipes start at 31501: [31501, 31501 + 500 * 32 = 47501) <= 47520
    int perm_ptr = 1;
    int temp_ptr = 31501;

    for (int k = 2; k <= 1000; ++k) {
        if (k % 2 == 0) {
            place(k, perm_ptr++, 1);
        } else {
            place(k, temp_ptr++, 1);
        }
    }

    for (int k = 1; k <= 1000; k += 2) {
        remove_p(k);
    }

    int pid = 1001;
    vector<int> sizes = {2, 4, 8, 16, 32};
    for (int S : sizes) {
        temp_ptr = 31501;
        for (int k = 1; k <= 1000; ++k) {
            if (k % 2 == 0) {
                place(pid++, perm_ptr, S);
                perm_ptr += S;
            } else {
                place(pid++, temp_ptr, S);
                temp_ptr += S;
            }
        }
        for (int k = 1; k <= 1000; k += 2) {
            int del_id = pid - 1000 + (k - 1);
            remove_p(del_id);
        }
    }

    int num_occ = 0;
    for (int c : cells) if (c > 0) num_occ++;
    double score = (double)num_occ / trench_len * w;
    cout << "Test 9 score: " << fixed << setprecision(4) << score << " (L=" << trench_len << ")" << endl;
    return score;
}

int main() {
    sim_test5();
    sim_test9();
    return 0;
}
