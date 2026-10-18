#include <iostream>
#include <vector>
#include <fstream>
#include <algorithm>
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

    vector<int> alive_prefix;
    for (int i = 1; i <= online_pipes; ++i) {
        if (pipes[i].death > 4999) {
            alive_prefix.push_back(i);
        }
    }

    // Best-fit evaluator:
    auto eval_order_best_fit = [&](const vector<int>& order, int min_rem_pref) -> pair<int, int> {
        auto p_copy = pipes;
        vector<int> placed = alive_prefix;
        int max_coord = trench;

        vector<pair<int, int>> intervals;
        intervals.reserve(placed.size() + order.size());

        for (int idx : order) {
            auto& p = p_copy[idx];
            intervals.clear();
            for (int p_idx : placed) {
                const auto& other = p_copy[p_idx];
                if (!(p.death <= other.birth || other.death <= p.birth)) {
                    intervals.emplace_back(other.shift, other.shift + other.len);
                }
            }
            sort(intervals.begin(), intervals.end());

            // Merge intervals
            vector<pair<int, int>> merged;
            for (const auto& iv : intervals) {
                if (merged.empty() || merged.back().second < iv.first) {
                    merged.push_back(iv);
                } else {
                    merged.back().second = max(merged.back().second, iv.second);
                }
            }

            // Available holes
            int chosen_s = -1;
            int best_waste = 1e9;
            int cur_x = 0;

            for (const auto& iv : merged) {
                int gap = iv.first - cur_x;
                if (gap >= p.len) {
                    int waste = gap - p.len;
                    // Prefer exact fit (waste == 0), then waste >= min_rem_pref, then smaller waste
                    int penalty = waste;
                    if (waste > 0 && waste < min_rem_pref) penalty += 10000; // avoid bad unusable slivers
                    if (penalty < best_waste) {
                        best_waste = penalty;
                        chosen_s = cur_x;
                        if (waste == 0) break; // Perfect!
                    }
                }
                cur_x = iv.second;
            }

            if (chosen_s == -1) {
                // Place at end
                chosen_s = cur_x;
            }

            p.shift = chosen_s;
            placed.push_back(idx);
            max_coord = max(max_coord, p.shift + p.len);
        }
        int tr = (max_coord + 19) / 20 * 20;
        return {tr, max_coord};
    };

    // First-fit evaluator for comparison:
    auto eval_order_first_fit = [&](const vector<int>& order) -> pair<int, int> {
        auto p_copy = pipes;
        vector<int> placed = alive_prefix;
        int max_coord = trench;

        vector<pair<int, int>> intervals;
        intervals.reserve(placed.size() + order.size());

        for (int idx : order) {
            auto& p = p_copy[idx];
            intervals.clear();
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
        int tr = (max_coord + 19) / 20 * 20;
        return {tr, max_coord};
    };

    vector<int> tail_pipes;
    for (int i = online_pipes + 1; i <= num_pipes; ++i) tail_pipes.push_back(i);

    // Test with the best weights from save_t11_clean:
    int dur_w = 2, len_w = 3, b_w = 2;
    sort(tail_pipes.begin(), tail_pipes.end(), [&](int a, int b) {
        bool perm_a = (pipes[a].death == 1000000000);
        bool perm_b = (pipes[b].death == 1000000000);
        if (perm_a != perm_b) return perm_a > perm_b;
        long long sc_a = (long long)dur_w * (pipes[a].death - pipes[a].birth) + (long long)len_w * pipes[a].len + (long long)b_w * pipes[a].birth;
        long long sc_b = (long long)dur_w * (pipes[b].death - pipes[b].birth) + (long long)len_w * pipes[b].len + (long long)b_w * pipes[b].birth;
        if (sc_a != sc_b) return sc_a > sc_b;
        return pipes[a].len > pipes[b].len;
    });

    auto [ff_tr, ff_mc] = eval_order_first_fit(tail_pipes);
    cout << "First-fit: Trench=" << ff_tr << ", max_coord=" << ff_mc << endl;

    for (int min_rem : {0, 8, 12, 16, 20}) {
        auto [bf_tr, bf_mc] = eval_order_best_fit(tail_pipes, min_rem);
        cout << "Best-fit (min_rem=" << min_rem << "): Trench=" << bf_tr << ", max_coord=" << bf_mc << endl;
    }

    return 0;
}
