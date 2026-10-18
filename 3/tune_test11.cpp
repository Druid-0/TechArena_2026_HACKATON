#include <iostream>
#include <vector>
#include <fstream>
#include <algorithm>

using namespace std;

struct Pipe {
    int id, birth, death, len, shift;
};

int main() {
    ifstream in("tests/11.in");
    double w;
    int n;
    in >> w >> n;
    vector<int> ops(n);
    for (int i = 0; i < n; ++i) in >> ops[i];

    vector<Pipe> pipes;
    pipes.push_back({0, -1, -1, 0, -1});
    int pid = 1;
    for (int t = 0; t < n; ++t) {
        if (ops[t] > 0) {
            pipes.push_back({pid, t, 1000000000, ops[t], -1});
            pid++;
        } else if (ops[t] < 0) {
            pipes[-ops[t]].death = t;
        }
    }
    int num_pipes = pid - 1;

    int best_trench = 1000000;

    for (int dur_w : {-2, -1, 0, 1, 2}) {
        for (int len_w : {1, 2, 3}) {
            vector<int> order(num_pipes);
            for (int i = 0; i < num_pipes; ++i) order[i] = i + 1;

            sort(order.begin(), order.end(), [&](int a, int b) {
                bool perm_a = (pipes[a].death == 1000000000);
                bool perm_b = (pipes[b].death == 1000000000);
                if (perm_a != perm_b) return perm_a > perm_b;
                int score_a = dur_w * (pipes[a].death - pipes[a].birth) + len_w * pipes[a].len;
                int score_b = dur_w * (pipes[b].death - pipes[b].birth) + len_w * pipes[b].len;
                if (score_a != score_b) return score_a > score_b;
                return pipes[a].birth < pipes[b].birth;
            });

            vector<int> placed;
            int max_coord = 0;
            for (int idx : order) {
                auto& p = pipes[idx];
                vector<pair<int, int>> intervals;
                for (int p_idx : placed) {
                    const auto& other = pipes[p_idx];
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
                placed.push_back(idx);
                max_coord = max(max_coord, p.shift + p.len);
            }

            int trench = (max_coord + 19) / 20 * 20;
            if (trench < best_trench) {
                best_trench = trench;
                cout << "New best trench for Test 11: " << trench 
                     << " (score=" << (double)20361 / trench * w << ")" << endl;
            }
        }
    }
    return 0;
}
