
#include <iostream>
#include <vector>
#include <fstream>
#include <algorithm>
#include <chrono>

using namespace std;

struct Pipe {
    int id, birth, death, len, shift;
};

int main() {
    ifstream in("tests/13.in");
    double w; int n;
    in >> w >> n;
    vector<int> ops(n);
    for (int i = 0; i < n; ++i) in >> ops[i];

    vector<Pipe> pipes(5159);
    int pid = 1;
    for (int t = 0; t < n; ++t) {
        if (ops[t] > 0) {
            pipes[pid] = {pid, t, 1000000000, ops[t], -1};
            pid++;
        } else if (ops[t] < 0) {
            pipes[-ops[t]].death = t;
        }
    }
    int num_pipes = pid - 1;

    auto eval_order = [&](const vector<int>& order) -> pair<int, vector<int>> {
        auto p_copy = pipes;
        vector<int> placed;
        int max_coord = 0;
        vector<int> shifts(num_pipes + 1, -1);

        for (int idx : order) {
            auto& p = p_copy[idx];
            vector<pair<int, int>> intervals;
            for (int p_idx : placed) {
                const auto& other = p_copy[p_idx];
                if (!(p.death <= other.birth || other.death <= p.birth)) {
                    intervals.emplace_back(other.shift, other.shift + other.len);
                }
            }
            sort(intervals.begin(), intervals.end());

            int cur_shift = 0;
            for (const auto& iv : intervals) {
                if (cur_shift + p.len <= iv.first) break;
                if (cur_shift < iv.second) cur_shift = max(cur_shift, iv.second);
            }
            p.shift = cur_shift;
            shifts[idx] = cur_shift;
            placed.push_back(idx);
            max_coord = max(max_coord, p.shift + p.len);
        }
        int tr = (max_coord + 19) / 20 * 20;
        return {tr, shifts};
    };

    vector<int> base_order(num_pipes);
    for (int i = 0; i < num_pipes; ++i) base_order[i] = i + 1;

    int best_trench = 3240;

    for (int perm_first : {1, 0}) {
        for (int dur_w : {0}) {
            for (int len_w : {0}) {
                for (int b_w : {0}) {
                    for (int d_w : {0}) {
                        auto order = base_order;
                        sort(order.begin(), order.end(), [&](int a, int b) {
                            if (perm_first) {
                                bool perm_a = (pipes[a].death == 1000000000);
                                bool perm_b = (pipes[b].death == 1000000000);
                                if (perm_a != perm_b) return perm_a > perm_b;
                            }
                            int sc_a = dur_w * (pipes[a].death - pipes[a].birth) + len_w * pipes[a].len + b_w * pipes[a].birth + d_w * (pipes[a].death % 100000);
                            int sc_b = dur_w * (pipes[b].death - pipes[b].birth) + len_w * pipes[b].len + b_w * pipes[b].birth + d_w * (pipes[b].death % 100000);
                            if (sc_a != sc_b) return sc_a > sc_b;
                            return pipes[a].len > pipes[b].len;
                        });
                        auto [tr, sh] = eval_order(order);
                        if (tr < best_trench) {
                            best_trench = tr;
                            cout << "New best T13: Trench=" << tr 
                                 << ", Score=" << (double)3192 / tr * w 
                                 << " (perm=" << perm_first << ", dur=" << dur_w 
                                 << ", len=" << len_w << ", b=" << b_w << ", d=" << d_w << ")" << endl;
                        }
                    }
                }
            }
        }
    }
    cout << "Done. Best trench for Test 13: " << best_trench << endl;
    return 0;
}
