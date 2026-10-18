
#include <iostream>
#include <vector>
#include <fstream>
#include <set>
#include <map>
#include <algorithm>
#include <cstdint>

using namespace std;

struct Pipe {
    int id, birth, death, len, shift;
};

int main() {
    ifstream in("tests/11.in");
    double w; int n;
    in >> w >> n;
    vector<int> ops(n);
    for (int i = 0; i < n; ++i) in >> ops[i];

    vector<uint8_t> perm_bits(16282);
    ifstream f_bits("perm_bits_15.bin", ios::binary);
    f_bits.read((char*)perm_bits.data(), 16282);

    auto is_perm = [&](int pid) -> bool {
        int idx = pid - 1;
        return (perm_bits[idx / 8] >> (idx % 8)) & 1;
    };

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

    // Run new allocator for first 4999 steps:
    map<int, int> by_start;
    set<pair<int, int>> by_len;
    int trench = 0;

    auto add_free = [&](int s, int l) {
        if (l <= 0) return;
        by_start[s] = l;
        by_len.insert({l, s});
    };
    auto remove_free = [&](int s, int l) {
        by_start.erase(s);
        by_len.erase({l, s});
    };

    pid = 1;
    for (int step = 0; step < 4999; ++step) {
        int op = ops[step];
        if (op > 0) {
            int req = op;
            bool perm = is_perm(pid);
            int best_s = -1, best_l = -1;

            if (perm) {
                auto it = by_len.lower_bound({req, -1});
                if (it != by_len.end() && it->first == req) {
                    best_s = it->second; best_l = it->first;
                } else {
                    auto it2 = by_len.lower_bound({req + 30, -1});
                    if (it2 != by_len.end()) {
                        best_s = it2->second; best_l = it2->first;
                    }
                }
            } else {
                auto it = by_len.lower_bound({req, -1});
                int found_s = -1, found_l = -1;
                int count = 0;
                for (; it != by_len.end() && count < 18; ++it, ++count) {
                    if (found_s == -1) { found_s = it->second; found_l = it->first; }
                    if (it->first - req >= 14 || it->first == req) {
                        best_s = it->second; best_l = it->first; break;
                    }
                }
                if (best_s == -1 && found_s != -1) {
                    best_s = found_s; best_l = found_l;
                }
            }

            if (best_s != -1) {
                remove_free(best_s, best_l);
                int rem = best_l - req;
                int place_shift = best_s;
                if (!perm && rem > 0) {
                    place_shift = best_s + rem;
                    add_free(best_s, rem);
                } else {
                    if (rem > 0) add_free(best_s + req, rem);
                }
                pipes[pid].shift = place_shift;
            } else {
                int trailing_s = trench;
                int trailing_len = 0;
                if (!by_start.empty()) {
                    auto last = prev(by_start.end());
                    if (last->first + last->second == trench) {
                        trailing_s = last->first;
                        trailing_len = last->second;
                        remove_free(trailing_s, trailing_len);
                    }
                }
                int needed = req - trailing_len;
                int cnt = (needed + 19) / 20;
                trench += cnt * 20;
                trailing_len += cnt * 20;
                int rem = trailing_len - req;
                if (rem > 0) add_free(trailing_s + req, rem);
                pipes[pid].shift = trailing_s;
            }
            pid++;
        } else {
            int id = -op;
            int s = pipes[id].shift;
            int l = pipes[id].len;

            auto it = by_start.lower_bound(s);
            int cur_s = s, cur_l = l;
            if (it != by_start.begin()) {
                auto left = prev(it);
                if (left->first + left->second == cur_s) {
                    cur_s = left->first;
                    cur_l += left->second;
                    remove_free(left->first, left->second);
                }
            }
            auto right = by_start.lower_bound(cur_s + cur_l);
            if (right != by_start.end() && right->first == cur_s + cur_l) {
                cur_l += right->second;
                remove_free(right->first, right->second);
            }
            add_free(cur_s, cur_l);
        }
    }

    int online_pipes = pid - 1;
    cout << "Test 11 prefix pipes=" << online_pipes << ", prefix trench=" << trench << endl;

    // Pack tail
    int best_trench = 100000;
    for (int dur_w : {-2, -1, 0, 1, 2}) {
        for (int len_w : {-2, -1, 0, 1, 2}) {
            auto p_copy = pipes;
            vector<int> placed;
            int max_coord = trench;
            for (int i = 1; i <= online_pipes; ++i) placed.push_back(i);

            vector<int> order;
            for (int i = online_pipes + 1; i <= num_pipes; ++i) order.push_back(i);

            sort(order.begin(), order.end(), [&](int a, int b) {
                bool perm_a = (p_copy[a].death == 1000000000);
                bool perm_b = (p_copy[b].death == 1000000000);
                if (perm_a != perm_b) return perm_a > perm_b;
                int s_a = dur_w * (p_copy[a].death - p_copy[a].birth) + len_w * p_copy[a].len;
                int s_b = dur_w * (p_copy[b].death - p_copy[b].birth) + len_w * p_copy[b].len;
                if (s_a != s_b) return s_a > s_b;
                return p_copy[a].len > p_copy[b].len;
            });

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
                placed.push_back(idx);
                max_coord = max(max_coord, p.shift + p.len);
            }

            int t_res = (max_coord + 19) / 20 * 20;
            if (t_res < best_trench) {
                best_trench = t_res;
                cout << "dur_w=" << dur_w << ", len_w=" << len_w << ": Trench=" << t_res 
                     << ", Score=" << (double)20361 / t_res * w << endl;
            }
        }
    }
    return 0;
}
