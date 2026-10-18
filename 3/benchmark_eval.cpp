
#include <iostream>
#include <vector>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <map>
#include <set>
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

    vector<Pipe> pipes(6494);
    for (int i = 1; i <= 6493; ++i) {
        pipes[i].id = i;
        pipes[i].birth = -1;
        pipes[i].death = 1000000000;
        pipes[i].len = 0;
        pipes[i].shift = -1;
    }

    int pid = 1;
    for (int t = 0; t < n; ++t) {
        if (ops[t] > 0) {
            pipes[pid].birth = t;
            pipes[pid].len = ops[t];
            pid++;
        } else if (ops[t] < 0) {
            pipes[-ops[t]].death = t;
        }
    }
    int num_pipes = pid - 1;

    // Simulate prefix exactly as in solution
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
                    int min_rem = (req <= 12 ? 8 : 16);
                    if (it->first - req >= min_rem || it->first == req) {
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

    int online_pipes = pid - 1; // 4006

    // Pre-filter prefix pipes: only keep those whose death > 4999
    vector<int> alive_prefix;
    for (int i = 1; i <= online_pipes; ++i) {
        if (pipes[i].death > 4999) {
            alive_prefix.push_back(i);
        }
    }
    cout << "Active prefix pipes at step 5000: " << alive_prefix.size() 
         << " (out of " << online_pipes << "), prefix trench: " << trench << endl;

    auto eval_order = [&](const vector<int>& order) -> pair<int, vector<int>> {
        auto p_copy = pipes;
        vector<int> placed = alive_prefix;
        int max_coord = trench;
        vector<int> shifts(num_pipes + 1, -1);
        for (int i : alive_prefix) shifts[i] = p_copy[i].shift;

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

    vector<int> base_order;
    for (int i = online_pipes + 1; i <= num_pipes; ++i) base_order.push_back(i);

    int best_trench = 22020;
    vector<int> best_shifts;

    // Grid search over priority weights
    for (int perm_first : {0, 1}) {
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
                            best_shifts = sh;
                            cout << "New best T11 tail: Trench=" << tr 
                                 << ", Score=" << (double)20361 / tr * w 
                                 << " (perm_first=" << perm_first << ", dur=" << dur_w << ", len=" << len_w 
                                 << ", b=" << b_w << ", d=" << d_w << ")" << endl;
                        }
                    }
                }
            }
        }
    }
    cout << "Grid search completed. Best trench: " << best_trench << endl;
    return 0;
}
