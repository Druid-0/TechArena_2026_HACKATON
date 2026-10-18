#include <iostream>
#include <vector>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <map>
#include <set>

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

    map<int, int> m_start;
    set<pair<int, int>> s_len;
    int trench = 0;

    auto add_free = [&](int s, int l) {
        if (l <= 0) return;
        m_start[s] = l;
        s_len.insert({l, s});
    };
    auto remove_free = [&](int s, int l) {
        m_start.erase(s);
        s_len.erase({l, s});
    };

    pid = 1;
    for (int step = 0; step < 4999; ++step) {
        int op = ops[step];
        if (op > 0) {
            int req = op;
            bool perm = is_perm(pid);
            int best_s = -1, best_l = -1;

            if (perm) {
                auto it = s_len.lower_bound({req, -1});
                if (it != s_len.end() && it->first == req) {
                    best_s = it->second; best_l = it->first;
                } else {
                    auto it2 = s_len.lower_bound({req + 30, -1});
                    if (it2 != s_len.end()) {
                        best_s = it2->second; best_l = it2->first;
                    }
                }
            } else {
                auto it = s_len.lower_bound({req, -1});
                int found_s = -1, found_l = -1;
                int count = 0;
                for (; it != s_len.end() && count < 18; ++it, ++count) {
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
                if (!m_start.empty()) {
                    auto last = prev(m_start.end());
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

            auto it = m_start.lower_bound(s);
            int cur_s = s, cur_l = l;
            if (it != m_start.begin()) {
                auto left = prev(it);
                if (left->first + left->second == cur_s) {
                    cur_s = left->first;
                    cur_l += left->second;
                    remove_free(left->first, left->second);
                }
            }
            auto right = m_start.lower_bound(cur_s + cur_l);
            if (right != m_start.end() && right->first == cur_s + cur_l) {
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

    auto eval_order = [&](const vector<int>& order) -> int {
        vector<int> placed = alive_prefix;
        vector<int> cur_shifts(num_pipes + 1, -1);
        for (int i : alive_prefix) cur_shifts[i] = pipes[i].shift;

        int max_coord = trench;
        vector<pair<int, int>> intervals;
        intervals.reserve(placed.size() + order.size());

        for (int idx : order) {
            const auto& p = pipes[idx];
            intervals.clear();
            for (int p_idx : placed) {
                const auto& other = pipes[p_idx];
                if (!(p.death <= other.birth || other.death <= p.birth)) {
                    int sh = cur_shifts[p_idx];
                    intervals.emplace_back(sh, sh + other.len);
                }
            }
            sort(intervals.begin(), intervals.end());

            int cur_shift = 0;
            for (const auto& iv : intervals) {
                if (cur_shift + p.len <= iv.first) break;
                if (cur_shift < iv.second) cur_shift = max(cur_shift, iv.second);
            }
            cur_shifts[idx] = cur_shift;
            placed.push_back(idx);
            max_coord = max(max_coord, cur_shift + p.len);
        }
        return (max_coord + 19) / 20 * 20;
    };

    vector<int> base_order;
    for (int i = online_pipes + 1; i <= num_pipes; ++i) base_order.push_back(i);

    auto t0 = chrono::high_resolution_clock::now();
    int tr = eval_order(base_order);
    auto t1 = chrono::high_resolution_clock::now();
    double ms = chrono::duration<double, milli>(t1 - t0).count();
    cout << "Base order trench: " << tr << ", elapsed: " << ms << " ms" << endl;

    return 0;
}
