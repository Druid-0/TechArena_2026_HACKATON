
#include <iostream>
#include <vector>
#include <fstream>
#include <algorithm>
#include <random>
#include <chrono>
#include <map>
#include <set>
#include <cstdint>
#include <cmath>

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

    vector<int> alive_prefix;
    for (int i = 1; i <= online_pipes; ++i) {
        if (pipes[i].death > 4999) {
            alive_prefix.push_back(i);
        }
    }

    const int TARGET = 20760;

    auto eval_order = [&](const vector<int>& order) -> pair<double, pair<int, vector<int>>> {
        auto p_copy = pipes;
        vector<int> placed = alive_prefix;
        int max_coord = trench;
        vector<int> shifts(num_pipes + 1, -1);
        for (int i : alive_prefix) shifts[i] = p_copy[i].shift;

        vector<pair<int, int>> intervals;
        intervals.reserve(placed.size() + order.size());

        double penalty = 0;

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
            shifts[idx] = cur_shift;
            placed.push_back(idx);

            int end_coord = cur_shift + p.len;
            if (end_coord > max_coord) max_coord = end_coord;
            if (end_coord > TARGET) {
                double diff = end_coord - TARGET;
                penalty += diff * diff;
            }
        }
        int tr = (max_coord + 19) / 20 * 20;
        double cost = tr * 10000.0 + penalty;
        return {cost, {max_coord, shifts}};
    };

    vector<int> tail_pipes;
    for (int i = online_pipes + 1; i <= num_pipes; ++i) tail_pipes.push_back(i);

    // Initial order from best known weights (dur=2, len=3, b=2, d=0):
    int dur_w = 2, len_w = 3, b_w = 2, d_w = 0;
    sort(tail_pipes.begin(), tail_pipes.end(), [&](int a, int b) {
        int sc_a = dur_w * (pipes[a].death - pipes[a].birth) + len_w * pipes[a].len + b_w * pipes[a].birth + d_w * (pipes[a].death % 100000);
        int sc_b = dur_w * (pipes[b].death - pipes[b].birth) + len_w * pipes[b].len + b_w * pipes[b].birth + d_w * (pipes[b].death % 100000);
        if (sc_a != sc_b) return sc_a > sc_b;
        return pipes[a].len > pipes[b].len;
    });

    auto [cur_cost, res] = eval_order(tail_pipes);
    auto [cur_coord, cur_shifts] = res;
    cout << "Initial T11 max_coord=" << cur_coord << ", Trench=" << (cur_coord + 19) / 20 * 20 
         << ", Cost=" << cur_cost << endl;

    double best_cost = cur_cost;
    int best_coord = cur_coord;
    auto best_shifts = cur_shifts;
    auto cur_order = tail_pipes;

    mt19937 rng(1337);
    uniform_int_distribution<int> jump_dist(1, 50);

    for (int step = 0; step < 5000; ++step) {
        vector<int> offenders;
        for (int i = 0; i < (int)cur_order.size(); ++i) {
            int idx = cur_order[i];
            if (cur_shifts[idx] + pipes[idx].len > TARGET) {
                offenders.push_back(i);
            }
        }
        if (offenders.empty()) {
            cout << "All pipes <= TARGET reached!" << endl;
            break;
        }

        int off_pos = offenders[rng() % offenders.size()];
        int jump = min(off_pos, jump_dist(rng));
        int target_pos = off_pos - jump;

        auto next_order = cur_order;
        int elem = next_order[off_pos];
        next_order.erase(next_order.begin() + off_pos);
        next_order.insert(next_order.begin() + target_pos, elem);

        auto [nc, n_res] = eval_order(next_order);
        auto [mc, sh] = n_res;

        double delta = nc - cur_cost;
        double T = max(100.0, 50000.0 * (1.0 - (double)step / 5000));

        if (delta <= 0 || uniform_real_distribution<double>(0, 1)(rng) < exp(-delta / T)) {
            cur_cost = nc;
            cur_coord = mc;
            cur_order = next_order;
            cur_shifts = sh;

            if (nc < best_cost) {
                best_cost = nc;
                best_coord = mc;
                best_shifts = sh;
                int tr = (mc + 19) / 20 * 20;
                cout << "Step " << step << ": NEW BEST max_coord=" << mc 
                     << ", Trench=" << tr 
                     << ", Cost=" << nc 
                     << ", Score=" << (double)20361 / tr * w << endl;
            }
        }

        if (step % 500 == 0) {
            cout << "Step " << step << ": cur_coord=" << cur_coord 
                 << ", best_coord=" << best_coord 
                 << ", best_cost=" << best_cost << endl;
        }
    }

    int final_tr = (best_coord + 19) / 20 * 20;
    cout << "Final best_coord: " << best_coord << ", Trench: " << final_tr << endl;
    if (final_tr < 21920) {
        ofstream out("plan11_tail.bin", ios::binary);
        int tail_count = num_pipes - online_pipes;
        out.write((char*)&tail_count, 4);
        for (int i = online_pipes + 1; i <= num_pipes; ++i) {
            out.write((char*)&best_shifts[i], 4);
        }
        out.close();
        cout << "Saved improved plan11_tail.bin!" << endl;
    }
    return 0;
}
