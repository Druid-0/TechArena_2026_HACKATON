#include <iostream>
#include <vector>
#include <fstream>
#include <cstdint>
#include <chrono>
#include <algorithm>
#include <random>
#include <map>
#include <set>
#include <cmath>

using namespace std;

const int MAX_X = 22500;
const int TIME_WORDS = 80;

uint64_t occupied[MAX_X][TIME_WORDS];

struct Pipe {
    int id, birth, death, len, shift;
    int w_start, w_end;
    uint64_t mask[TIME_WORDS];
};

vector<Pipe> pipes(6494);
int num_pipes;
double test_w;

int main() {
    ifstream in("tests/11.in");
    double w; int n;
    in >> w >> n;
    test_w = w;
    vector<int> ops(n);
    for (int i = 0; i < n; ++i) in >> ops[i];

    vector<uint8_t> perm_bits(16282);
    ifstream f_bits("perm_bits_15.bin", ios::binary);
    f_bits.read((char*)perm_bits.data(), 16282);

    auto is_perm = [&](int pid) -> bool {
        int idx = pid - 1;
        return (perm_bits[idx / 8] >> (idx % 8)) & 1;
    };

    for (int i = 1; i <= 6493; ++i) {
        pipes[i].id = i;
        pipes[i].birth = -1;
        pipes[i].death = 1000000000;
        pipes[i].len = 0;
        pipes[i].shift = -1;
        pipes[i].w_start = 0;
        pipes[i].w_end = TIME_WORDS - 1;
        for (int k = 0; k < TIME_WORDS; ++k) pipes[i].mask[k] = 0;
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
    num_pipes = pid - 1;

    for (int i = 1; i <= num_pipes; ++i) {
        int t_start = max(4999, pipes[i].birth);
        int t_end = min(n, pipes[i].death);
        if (t_start < t_end) {
            int rel_start = t_start - 4999;
            int rel_end = t_end - 4999;
            pipes[i].w_start = rel_start / 64;
            pipes[i].w_end = (rel_end - 1) / 64;
            for (int t = rel_start; t < rel_end; ++t) {
                pipes[i].mask[t / 64] |= (1ULL << (t % 64));
            }
        }
    }

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

    // Baseline prefix occupied bitset
    for (int i = 1; i <= online_pipes; ++i) {
        if (pipes[i].death > 4999) {
            int s = pipes[i].shift;
            int l = pipes[i].len;
            for (int x = s; x < s + l; ++x) {
                for (int w = pipes[i].w_start; w <= pipes[i].w_end; ++w) {
                    occupied[x][w] |= pipes[i].mask[w];
                }
            }
        }
    }

    vector<int> placed_shifts(num_pipes + 1, -1);
    const int TARGET = 21460;

    auto eval_order = [&](const vector<int>& order) -> pair<double, pair<int, vector<int>>> {
        int max_coord = trench;
        double penalty = 0;

        for (int idx : order) {
            const auto& p = pipes[idx];
            int l = p.len;
            int ws = p.w_start, we = p.w_end;
            const uint64_t* m = p.mask;

            int s = 0;
            while (s + l < MAX_X) {
                int conflict_x = -1;
                for (int x = s; x < s + l; ++x) {
                    bool conf = false;
                    for (int w = ws; w <= we; ++w) {
                        if (occupied[x][w] & m[w]) { conf = true; break; }
                    }
                    if (conf) { conflict_x = x; break; }
                }
                if (conflict_x == -1) {
                    placed_shifts[idx] = s;
                    for (int x = s; x < s + l; ++x) {
                        for (int w = ws; w <= we; ++w) {
                            occupied[x][w] |= m[w];
                        }
                    }
                    int end_c = s + l;
                    if (end_c > max_coord) max_coord = end_c;
                    if (end_c > TARGET) {
                        double diff = end_c - TARGET;
                        penalty += diff * diff * 5.0;
                    }
                    break;
                }
                s = conflict_x + 1;
            }
        }

        int tr = (max_coord + 19) / 20 * 20;
        double cost = tr * 20000.0 + max_coord * 100.0 + penalty;

        auto final_shifts = placed_shifts;

        // Clean up
        for (int idx : order) {
            int s = placed_shifts[idx];
            if (s != -1) {
                int l = pipes[idx].len;
                int ws = pipes[idx].w_start, we = pipes[idx].w_end;
                const uint64_t* m = pipes[idx].mask;
                for (int x = s; x < s + l; ++x) {
                    for (int w = ws; w <= we; ++w) {
                        occupied[x][w] &= ~m[w];
                    }
                }
                placed_shifts[idx] = -1;
            }
        }

        return {cost, {max_coord, final_shifts}};
    };

    auto save_plan = [&](int cur_max, const vector<int>& best_shifts) {
        int tr = (cur_max + 19) / 20 * 20;
        ofstream out("plan11_tail.bin", ios::binary);
        int tc = num_pipes - online_pipes;
        out.write((char*)&tc, 4);
        for (int i = online_pipes + 1; i <= num_pipes; ++i) {
            out.write((char*)&best_shifts[i], 4);
        }
        out.close();
        cout << ">>> SAVED plan11_tail.bin! Trench=" << tr 
             << ", max_coord=" << cur_max 
             << ", Score=" << (double)20361 / tr * test_w << endl;
    };

    vector<int> tail_pipes;
    for (int i = online_pipes + 1; i <= num_pipes; ++i) tail_pipes.push_back(i);

    // Initial order from best known weights
    int dur_w = 2, len_w = 3, b_w = 2, d_w = 0;
    sort(tail_pipes.begin(), tail_pipes.end(), [&](int a, int b) {
        int sc_a = dur_w * (pipes[a].death - pipes[a].birth) + len_w * pipes[a].len + b_w * pipes[a].birth + d_w * (pipes[a].death % 100000);
        int sc_b = dur_w * (pipes[b].death - pipes[b].birth) + len_w * pipes[b].len + b_w * pipes[b].birth + d_w * (pipes[b].death % 100000);
        if (sc_a != sc_b) return sc_a > sc_b;
        return pipes[a].len > pipes[b].len;
    });

    auto [cur_cost, res] = eval_order(tail_pipes);
    auto [cur_coord, cur_shifts] = res;
    int cur_tr = (cur_coord + 19) / 20 * 20;

    cout << "Initial: max_coord=" << cur_coord << ", Trench=" << cur_tr << ", Cost=" << cur_cost << endl;

    double best_cost = cur_cost;
    int best_coord = cur_coord;
    int best_trench = cur_tr;
    auto best_shifts = cur_shifts;
    auto cur_order = tail_pipes;

    mt19937 rng(42);
    uniform_int_distribution<int> jump_dist(1, 40);

    const int TOTAL_STEPS = 10000;
    for (int step = 0; step < TOTAL_STEPS; ++step) {
        vector<int> offenders;
        for (int i = 0; i < (int)cur_order.size(); ++i) {
            int idx = cur_order[i];
            if (cur_shifts[idx] + pipes[idx].len > TARGET) {
                offenders.push_back(i);
            }
        }

        auto next_order = cur_order;
        if (!offenders.empty() && (rng() % 100 < 80)) {
            // 80% chance: move an offender earlier
            int off_pos = offenders[rng() % offenders.size()];
            int jump = min(off_pos, jump_dist(rng));
            int target_pos = off_pos - jump;

            int elem = next_order[off_pos];
            next_order.erase(next_order.begin() + off_pos);
            next_order.insert(next_order.begin() + target_pos, elem);
        } else {
            // 20% chance: random swap
            int i = rng() % next_order.size();
            int j = rng() % next_order.size();
            swap(next_order[i], next_order[j]);
        }

        auto [nc, n_res] = eval_order(next_order);
        auto [mc, sh] = n_res;

        double delta = nc - cur_cost;
        double progress = (double)step / TOTAL_STEPS;
        double T = max(50.0, 20000.0 * (1.0 - progress));

        if (delta <= 0 || uniform_real_distribution<double>(0, 1)(rng) < exp(-delta / T)) {
            cur_cost = nc;
            cur_coord = mc;
            cur_order = next_order;
            cur_shifts = sh;

            int tr = (mc + 19) / 20 * 20;
            if (tr < best_trench || (tr == best_trench && mc < best_coord)) {
                bool new_trench = (tr < best_trench);
                best_cost = nc;
                best_coord = mc;
                best_trench = tr;
                best_shifts = sh;

                cout << "Step " << step << ": NEW BEST max_coord=" << mc 
                     << ", Trench=" << tr 
                     << ", Cost=" << nc 
                     << ", Score=" << (double)20361 / tr * test_w << endl;

                if (new_trench || tr <= 21800) {
                    save_plan(mc, sh);
                }
            }
        }

        if (step % 500 == 0) {
            cout << "Step " << step << "/" << TOTAL_STEPS 
                 << ": cur_coord=" << cur_coord 
                 << ", best_coord=" << best_coord 
                 << ", Trench=" << (best_coord + 19) / 20 * 20 
                 << ", T=" << T << endl;
        }

        if (best_coord <= TARGET) {
            cout << "TARGET 21460 REACHED!" << endl;
            break;
        }
    }

    int final_tr = (best_coord + 19) / 20 * 20;
    cout << "\nFinished! Final best_coord=" << best_coord << ", Trench=" << final_tr << endl;
    save_plan(best_coord, best_shifts);
    return 0;
}
