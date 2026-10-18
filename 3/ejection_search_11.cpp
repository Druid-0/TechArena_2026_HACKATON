
#include <iostream>
#include <vector>
#include <fstream>
#include <map>
#include <set>
#include <cstdint>
#include <algorithm>
#include <random>
#include <chrono>

using namespace std;

struct Pipe {
    int id, birth, death, len, shift;
};

const int MAX_T = 10000;
const int MAX_X = 22500;

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

    ifstream f_plan("plan11_tail.bin", ios::binary);
    int tail_count;
    f_plan.read((char*)&tail_count, 4);
    vector<int> tail_shifts(tail_count);
    f_plan.read((char*)tail_shifts.data(), tail_count * 4);

    for (int i = 0; i < tail_count; ++i) {
        pipes[4007 + i].shift = tail_shifts[i];
    }

    // Allocate 2D grid on heap
    vector<int> grid_data(5000 * MAX_X, 0);
    auto at = [&](int t, int x) -> int& {
        return grid_data[(t - 5000) * MAX_X + x];
    };

    auto place = [&](int p_id, int s) {
        pipes[p_id].shift = s;
        int t_start = max(5000, pipes[p_id].birth);
        int t_end = min(MAX_T, pipes[p_id].death);
        int l = pipes[p_id].len;
        for (int t = t_start; t < t_end; ++t) {
            for (int x = s; x < s + l; ++x) {
                at(t, x) = p_id;
            }
        }
    };

    auto unplace = [&](int p_id) {
        int s = pipes[p_id].shift;
        pipes[p_id].shift = -1;
        int t_start = max(5000, pipes[p_id].birth);
        int t_end = min(MAX_T, pipes[p_id].death);
        int l = pipes[p_id].len;
        for (int t = t_start; t < t_end; ++t) {
            for (int x = s; x < s + l; ++x) {
                at(t, x) = 0;
            }
        }
    };

    // Populate prefix obstacles:
    for (int i = 1; i <= 4006; ++i) {
        if (pipes[i].death > 4999) {
            int s = pipes[i].shift;
            int l = pipes[i].len;
            int t_start = 5000;
            int t_end = min(MAX_T, pipes[i].death);
            for (int t = t_start; t < t_end; ++t) {
                for (int x = s; x < s + l; ++x) {
                    at(t, x) = -1; // immutable
                }
            }
        }
    }

    // Populate initial tail:
    for (int i = 4007; i <= num_pipes; ++i) {
        place(i, pipes[i].shift);
    }

    cout << "Grid initialized successfully." << endl;

    int cur_max = 0;
    for (int i = 4007; i <= num_pipes; ++i) {
        cur_max = max(cur_max, pipes[i].shift + pipes[i].len);
    }
    cout << "Initial max coord: " << cur_max << " Trench: " << (cur_max + 19) / 20 * 20 << endl;

    // Fast conflict checking
    // Returns: -1 if immutable prefix obstacle hit,
    // otherwise list of unique tail pipe IDs conflicting
    auto check_conflicts = [&](int p_id, int s, int max_conf_allowed) -> pair<bool, vector<int>> {
        const auto& p = pipes[p_id];
        int t_start = max(5000, p.birth);
        int t_end = min(MAX_T, p.death);
        int l = p.len;
        vector<int> confs;

        for (int t = t_start; t < t_end; ++t) {
            for (int x = s; x < s + l; ++x) {
                int occ = at(t, x);
                if (occ == -1) return {true, {}}; // prefix obstacle
                if (occ > 0 && occ != p_id) {
                    bool already = false;
                    for (int c : confs) if (c == occ) { already = true; break; }
                    if (!already) {
                        confs.push_back(occ);
                        if ((int)confs.size() > max_conf_allowed) return {false, confs};
                    }
                }
            }
        }
        return {false, confs};
    };

    mt19937 rng(42);

    auto save_plan = [&]() {
        ofstream out("plan11_tail.bin", ios::binary);
        int tc = num_pipes - 4006;
        out.write((char*)&tc, 4);
        for (int i = 4007; i <= num_pipes; ++i) {
            out.write((char*)&pipes[i].shift, 4);
        }
        out.close();
        cout << "Saved plan11_tail.bin (Trench=" << (cur_max + 19) / 20 * 20 
             << ", max_coord=" << cur_max << ")!" << endl;
    };

    int best_trench = (cur_max + 19) / 20 * 20;

    for (int iter = 0; iter < 100000; ++iter) {
        // Find all bottleneck pipes
        vector<int> bottlenecks;
        int target_bound = cur_max - 1;
        for (int i = 4007; i <= num_pipes; ++i) {
            if (pipes[i].shift + pipes[i].len >= cur_max) {
                bottlenecks.push_back(i);
            }
        }

        if (bottlenecks.empty()) {
            cur_max--;
            int tr = (cur_max + 19) / 20 * 20;
            cout << "Iter " << iter << ": NEW MAX COORD: " << cur_max 
                 << ", Trench=" << tr 
                 << ", Score=" << (double)20361 / tr * w << endl;
            if (tr < best_trench) {
                best_trench = tr;
                save_plan();
            }
            continue;
        }

        int target_p = bottlenecks[rng() % bottlenecks.size()];
        int p_len = pipes[target_p].len;
        int old_shift = pipes[target_p].shift;

        unplace(target_p);

        // Try direct fit first (0 conflicts)
        bool solved = false;
        vector<int> cand_shifts;
        for (int s = 0; s <= target_bound - p_len; ++s) {
            cand_shifts.push_back(s);
        }
        shuffle(cand_shifts.begin(), cand_shifts.end(), rng);

        for (int s : cand_shifts) {
            auto [prefix_conf, confs] = check_conflicts(target_p, s, 0);
            if (!prefix_conf && confs.empty()) {
                place(target_p, s);
                solved = true;
                // cout << "Direct move: pipe " << target_p << " from " << old_shift << " to " << s << endl;
                break;
            }
        }

        if (!solved) {
            // Try 1-ejection
            for (int s : cand_shifts) {
                auto [prefix_conf, confs] = check_conflicts(target_p, s, 1);
                if (!prefix_conf && confs.size() == 1) {
                    int displaced = confs[0];
                    if (displaced <= 4006) continue;

                    int old_disp_shift = pipes[displaced].shift;
                    unplace(displaced);
                    place(target_p, s);

                    // Try to place displaced pipe elsewhere
                    int d_len = pipes[displaced].len;
                    bool disp_placed = false;

                    for (int ds = 0; ds <= target_bound - d_len; ++ds) {
                        auto [d_pref, d_confs] = check_conflicts(displaced, ds, 0);
                        if (!d_pref && d_confs.empty()) {
                            place(displaced, ds);
                            disp_placed = true;
                            solved = true;
                            break;
                        }
                    }

                    if (disp_placed) {
                        // Success!
                        break;
                    } else {
                        // Revert
                        unplace(target_p);
                        place(displaced, old_disp_shift);
                    }
                }
            }
        }

        if (!solved) {
            // Put target_p back
            place(target_p, old_shift);

            // Shuffling move: pick a random tail pipe and move it to any valid position
            // to shake up the landscape!
            int rand_p = 4007 + (rng() % (num_pipes - 4006));
            int r_len = pipes[rand_p].len;
            int r_old = pipes[rand_p].shift;
            unplace(rand_p);

            bool r_placed = false;
            for (int s : cand_shifts) {
                if (s + r_len <= target_bound) {
                    auto [r_pref, r_confs] = check_conflicts(rand_p, s, 0);
                    if (!r_pref && r_confs.empty()) {
                        place(rand_p, s);
                        r_placed = true;
                        break;
                    }
                }
            }
            if (!r_placed) {
                place(rand_p, r_old);
            }
        }

        if (iter % 1000 == 0) {
            cout << "Iter " << iter << ": cur_max=" << cur_max 
                 << ", bottlenecks count=" << bottlenecks.size() << endl;
        }
    }

    int final_tr = (cur_max + 19) / 20 * 20;
    cout << "Done! Final max coord: " << cur_max << " Trench: " << final_tr << endl;
    if (final_tr < best_trench) {
        save_plan();
    }
    return 0;
}
