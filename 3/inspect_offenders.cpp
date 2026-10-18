
#include <iostream>
#include <vector>
#include <fstream>
#include <map>
#include <set>
#include <cstdint>
#include <algorithm>

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

    // Load plan11_tail.bin
    ifstream f_plan("plan11_tail.bin", ios::binary);
    int tail_count;
    f_plan.read((char*)&tail_count, 4);
    vector<int> tail_shifts(tail_count);
    f_plan.read((char*)tail_shifts.data(), tail_count * 4);

    for (int i = 0; i < tail_count; ++i) {
        pipes[4007 + i].shift = tail_shifts[i];
    }

    const int LIMIT = 21460;

    // For each pipe that ends > LIMIT:
    for (int i = 4007; i <= num_pipes; ++i) {
        if (pipes[i].shift + pipes[i].len > LIMIT) {
            const auto& p = pipes[i];
            // Find all intervals in [0, LIMIT) that are occupied during [p.birth, p.death)
            vector<pair<int, int>> occ;
            for (int j = 1; j <= num_pipes; ++j) {
                if (j == i) continue;
                const auto& other = pipes[j];
                if (other.shift == -1 || other.shift >= LIMIT) continue;
                if (!(p.death <= other.birth || other.death <= p.birth)) {
                    occ.push_back({other.shift, min(LIMIT, other.shift + other.len)});
                }
            }
            sort(occ.begin(), occ.end());

            // Merge occupied
            vector<pair<int, int>> merged;
            for (auto iv : occ) {
                if (merged.empty() || merged.back().second < iv.first) {
                    merged.push_back(iv);
                } else {
                    merged.back().second = max(merged.back().second, iv.second);
                }
            }

            // Find free intervals in [0, LIMIT)
            int cur = 0;
            vector<pair<int, int>> free_ivs;
            for (auto iv : merged) {
                if (iv.first > cur) {
                    free_ivs.push_back({cur, iv.first - cur});
                }
                cur = max(cur, iv.second);
            }
            if (cur < LIMIT) free_ivs.push_back({cur, LIMIT - cur});

            int max_free_len = 0;
            int total_free_cells = 0;
            for (auto f : free_ivs) {
                max_free_len = max(max_free_len, f.second);
                total_free_cells += f.second;
            }

            cout << "Pipe " << i << " (len=" << p.len << ", b=" << p.birth << ", d=" << p.death 
                 << ", cur_shift=" << p.shift << "): "
                 << "total_free=" << total_free_cells 
                 << ", max_hole=" << max_free_len << endl;
        }
    }
    return 0;
}
