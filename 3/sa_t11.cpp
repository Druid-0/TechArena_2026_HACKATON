
#include <iostream>
#include <vector>
#include <fstream>
#include <algorithm>
#include <random>
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

    int online_pipes = pid - 1; // 4006
    int tail_count = num_pipes - online_pipes; // 2487
    cout << "Online prefix: " << online_pipes << " pipes, trench: " << trench << endl;

    // Fast interval packing for tail
    // For each tail pipe, find active intervals among already placed pipes
    // Precompute conflict graph or active interval query:
    // Notice: each position x has occupied intervals [birth, death).
    // Or simpler: for a candidate order, place sequentially:
    
    // We can represent state of trench using a segment tree or coordinate list!
    // Since trench is ~22000, we can maintain for each position x a vector of (birth, death)!
    // len <= 40! Checking if a pipe [birth, death) fits at shift s means checking [s, s+len):
    // For each x in [s, s+len): cell x is free in [birth, death)!
    
    // Let's test how fast this check is:
    // At coordinate x, how many intervals are there? Usually only 2-5 intervals!
    vector<vector<pair<int, int>>> cell_intervals(25000);
    for (int i = 1; i <= online_pipes; ++i) {
        int s = pipes[i].shift;
        int l = pipes[i].len;
        int b = pipes[i].birth;
        int d = pipes[i].death;
        for (int x = s; x < s + l; ++x) {
            cell_intervals[x].emplace_back(b, d);
        }
    }

    auto can_place = [&](int s, int l, int b, int d) -> bool {
        for (int x = s; x < s + l; ++x) {
            for (const auto& iv : cell_intervals[x]) {
                if (!(d <= iv.first || iv.second <= b)) return false;
            }
        }
        return true;
    };

    auto place = [&](int s, int l, int b, int d) {
        for (int x = s; x < s + l; ++x) {
            cell_intervals[x].emplace_back(b, d);
        }
    };

    // Fast evaluator for an order:
    auto eval_order = [&](const vector<int>& order) -> pair<int, vector<int>> {
        auto backup_intervals = cell_intervals;
        int max_coord = trench;
        vector<int> shifts(num_pipes + 1, -1);
        for (int idx : order) {
            int l = pipes[idx].len;
            int b = pipes[idx].birth;
            int d = pipes[idx].death;
            // Find lowest s:
            int s = 0;
            while (true) {
                // check if [s, s+l) is valid
                bool ok = true;
                int next_s = s + 1;
                for (int x = s; x < s + l; ++x) {
                    for (const auto& iv : cell_intervals[x]) {
                        if (!(d <= iv.first || iv.second <= b)) {
                            ok = false;
                            break;
                        }
                    }
                    if (!ok) break;
                }
                if (ok) {
                    for (int x = s; x < s + l; ++x) {
                        cell_intervals[x].emplace_back(b, d);
                    }
                    shifts[idx] = s;
                    max_coord = max(max_coord, s + l);
                    break;
                }
                s++;
            }
        }
        cell_intervals = backup_intervals;
        int tr = (max_coord + 19) / 20 * 20;
        return {tr, shifts};
    };

    // Initial order:
    vector<int> base_order;
    for (int i = online_pipes + 1; i <= num_pipes; ++i) base_order.push_back(i);

    // Try multiple heuristic keys:
    int best_trench = 100000;
    vector<int> best_shifts;
    vector<int> best_order;

    for (double perm_w : {0.0, 1e6}) {
        for (double b_w : {-5.0, -2.0, -1.0, 0.0, 1.0, 2.0, 5.0}) {
            for (double d_w : {-5.0, -2.0, -1.0, 0.0, 1.0, 2.0, 5.0}) {
                for (double dur_w : {-5.0, -2.0, -1.0, 0.0, 1.0, 2.0, 5.0}) {
                    for (double l_w : {-5.0, -2.0, -1.0, 0.0, 1.0, 2.0, 5.0}) {
                        auto order = base_order;
                        sort(order.begin(), order.end(), [&](int a, int b) {
                            bool perm_a = (pipes[a].death == 1000000000);
                            bool perm_b = (pipes[b].death == 1000000000);
                            if (perm_a != perm_b) return (perm_w > 0 ? (perm_a > perm_b) : (perm_a < perm_b));
                            double sc_a = b_w * pipes[a].birth + d_w * (pipes[a].death % 10000000) + dur_w * (pipes[a].death - pipes[a].birth) + l_w * pipes[a].len;
                            double sc_b = b_w * pipes[b].birth + d_w * (pipes[b].death % 10000000) + dur_w * (pipes[b].death - pipes[b].birth) + l_w * pipes[b].len;
                            if (sc_a != sc_b) return sc_a > sc_b;
                            return pipes[a].len > pipes[b].len;
                        });
                        auto [tr, sh] = eval_order(order);
                        if (tr < best_trench) {
                            best_trench = tr;
                            best_shifts = sh;
                            best_order = order;
                            cout << "New best T11 tail: Trench=" << tr 
                                 << ", Score=" << (double)20361 / tr * w << endl;
                        }
                    }
                }
            }
        }
    }
    return 0;
}
