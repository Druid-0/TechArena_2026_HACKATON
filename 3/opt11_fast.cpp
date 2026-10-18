
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

    ifstream f_plan("plan11_tail.bin", ios::binary);
    int tail_count;
    f_plan.read((char*)&tail_count, 4);
    vector<int> tail_shifts(tail_count);
    f_plan.read((char*)tail_shifts.data(), tail_count * 4);

    for (int i = 0; i < tail_count; ++i) {
        pipes[4007 + i].shift = tail_shifts[i];
    }

    // Precompute time_neighbors:
    vector<vector<int>> time_neighbors(num_pipes + 1);
    for (int i = 1; i <= num_pipes; ++i) {
        for (int j = 1; j <= num_pipes; ++j) {
            if (i == j) continue;
            if (!(pipes[i].death <= pipes[j].birth || pipes[j].death <= pipes[i].birth)) {
                // Only keep j if j is alive after step 4999
                if (pipes[j].death > 4999) {
                    time_neighbors[i].push_back(j);
                }
            }
        }
    }

    cout << "Precomputed time_neighbors successfully." << endl;

    int cur_max = 0;
    for (int i = 4007; i <= num_pipes; ++i) {
        cur_max = max(cur_max, pipes[i].shift + pipes[i].len);
    }
    cout << "Starting max coord: " << cur_max << " Trench: " << (cur_max + 19) / 20 * 20 << endl;

    // Fast conflict checking using time_neighbors:
    // Returns pair<has_prefix_conf, conf_tails>
    auto get_conflicts = [&](int p_id, int s, int max_tail_allowed) -> pair<bool, vector<int>> {
        int e = s + pipes[p_id].len;
        vector<int> conf_tails;
        for (int j : time_neighbors[p_id]) {
            int sj = pipes[j].shift;
            if (sj == -1) continue;
            int ej = sj + pipes[j].len;
            if (max(s, sj) < min(e, ej)) {
                if (j <= 4006) return {true, {}}; // immutable prefix conflict!
                conf_tails.push_back(j);
                if ((int)conf_tails.size() > max_tail_allowed) {
                    return {false, conf_tails};
                }
            }
        }
        return {false, conf_tails};
    };

    auto get_candidates = [&](int p_id, int max_bound) -> vector<int> {
        vector<int> cands = {0};
        int l = pipes[p_id].len;
        for (int j : time_neighbors[p_id]) {
            int sj = pipes[j].shift;
            if (sj == -1) continue;
            int s = sj + pipes[j].len;
            if (s + l <= max_bound) {
                cands.push_back(s);
            }
        }
        sort(cands.begin(), cands.end());
        cands.erase(unique(cands.begin(), cands.end()), cands.end());
        return cands;
    };

    auto save_plan = [&]() {
        ofstream out("plan11_tail.bin", ios::binary);
        int tc = num_pipes - 4006;
        out.write((char*)&tc, 4);
        for (int i = 4007; i <= num_pipes; ++i) {
            out.write((char*)&pipes[i].shift, 4);
        }
        out.close();
        int tr = (cur_max + 19) / 20 * 20;
        cout << "Saved plan11_tail.bin (Trench=" << tr 
             << ", max_coord=" << cur_max 
             << ", Score=" << (double)20361 / tr * w << ")!" << endl;
    };

    int best_trench = (cur_max + 19) / 20 * 20;
    save_plan(); // Save initial 21900 state

    mt19937 rng(42);

    for (int iter = 0; iter < 500000; ++iter) {
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
        int old_s = pipes[target_p].shift;
        pipes[target_p].shift = -1;

        auto cands = get_candidates(target_p, target_bound);
        shuffle(cands.begin(), cands.end(), rng);

        bool solved = false;
        // Direct fit
        for (int s : cands) {
            auto [has_pref, confs] = get_conflicts(target_p, s, 0);
            if (!has_pref && confs.empty()) {
                pipes[target_p].shift = s;
                solved = true;
                break;
            }
        }

        // 1-ejection
        if (!solved) {
            for (int s : cands) {
                auto [has_pref, confs] = get_conflicts(target_p, s, 1);
                if (!has_pref && confs.size() == 1) {
                    int disp = confs[0];
                    int old_disp_s = pipes[disp].shift;
                    pipes[disp].shift = -1;
                    pipes[target_p].shift = s;

                    auto disp_cands = get_candidates(disp, target_bound);
                    shuffle(disp_cands.begin(), disp_cands.end(), rng);

                    bool disp_placed = false;
                    for (int ds : disp_cands) {
                        auto [d_pref, d_confs] = get_conflicts(disp, ds, 0);
                        if (!d_pref && d_confs.empty()) {
                            pipes[disp].shift = ds;
                            disp_placed = true;
                            solved = true;
                            break;
                        }
                    }

                    if (disp_placed) {
                        break;
                    } else {
                        pipes[target_p].shift = -1;
                        pipes[disp].shift = old_disp_s;
                    }
                }
            }
        }

        if (!solved) {
            pipes[target_p].shift = old_s;

            // Perturbation: pick a random tail pipe and try to move it to a lower shift
            int rand_p = 4007 + (rng() % (num_pipes - 4006));
            int r_old = pipes[rand_p].shift;
            pipes[rand_p].shift = -1;

            auto r_cands = get_candidates(rand_p, target_bound);
            shuffle(r_cands.begin(), r_cands.end(), rng);

            bool r_placed = false;
            for (int s : r_cands) {
                if (s < r_old) { // only move lower
                    auto [r_pref, r_confs] = get_conflicts(rand_p, s, 0);
                    if (!r_pref && r_confs.empty()) {
                        pipes[rand_p].shift = s;
                        r_placed = true;
                        break;
                    }
                }
            }
            if (!r_placed) {
                pipes[rand_p].shift = r_old;
            }
        }

        if (iter % 5000 == 0) {
            cout << "Iter " << iter << ": cur_max=" << cur_max 
                 << ", bottlenecks count=" << bottlenecks.size() << endl;
        }
    }

    int final_tr = (cur_max + 19) / 20 * 20;
    cout << "Finished! Final max coord: " << cur_max << " Trench: " << final_tr << endl;
    if (final_tr < best_trench) {
        save_plan();
    }
    return 0;
}
