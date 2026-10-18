#include <iostream>
#include <vector>
#include <fstream>
#include <cstdint>
#include <algorithm>
#include <map>
#include <set>

using namespace std;

const int MAX_X = 22000;
const int TIME_WORDS = 80;

uint64_t occupied[MAX_X][TIME_WORDS];
vector<int> pipes_at_x[MAX_X];

struct Pipe {
    int id, birth, death, len, shift;
    int w_start, w_end;
    uint64_t mask[TIME_WORDS];
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
    int num_pipes = pid - 1;

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

    ifstream f_plan("plan11_tail.bin", ios::binary);
    int tail_count;
    f_plan.read((char*)&tail_count, 4);
    vector<int> tail_shifts(tail_count);
    f_plan.read((char*)tail_shifts.data(), tail_count * 4);

    for (int i = 0; i < tail_count; ++i) {
        pipes[4007 + i].shift = tail_shifts[i];
    }

    auto remove_pipe = [&](int id) {
        int s = pipes[id].shift;
        int l = pipes[id].len;
        for (int x = s; x < s + l; ++x) {
            for (int w = pipes[id].w_start; w <= pipes[id].w_end; ++w) {
                occupied[x][w] &= ~pipes[id].mask[w];
            }
            auto& vec = pipes_at_x[x];
            vec.erase(remove(vec.begin(), vec.end(), id), vec.end());
        }
        pipes[id].shift = -1;
    };

    auto add_pipe = [&](int id, int s) {
        int l = pipes[id].len;
        for (int x = s; x < s + l; ++x) {
            for (int w = pipes[id].w_start; w <= pipes[id].w_end; ++w) {
                occupied[x][w] |= pipes[id].mask[w];
            }
            pipes_at_x[x].push_back(id);
        }
        pipes[id].shift = s;
    };

    for (int i = 1; i <= num_pipes; ++i) {
        if (pipes[i].death > 4999) {
            add_pipe(i, pipes[i].shift);
        }
    }

    auto find_fit = [&](int id, int max_limit) -> int {
        int l = pipes[id].len;
        int ws = pipes[id].w_start, we = pipes[id].w_end;
        const uint64_t* m = pipes[id].mask;

        int s = 0;
        while (s + l <= max_limit) {
            int conf_x = -1;
            for (int x = s; x < s + l; ++x) {
                bool conf = false;
                for (int w = ws; w <= we; ++w) {
                    if (occupied[x][w] & m[w]) { conf = true; break; }
                }
                if (conf) { conf_x = x; break; }
            }
            if (conf_x == -1) return s;
            s = conf_x + 1;
        }
        return -1;
    };

    auto get_conflicts = [&](int id, int s) -> pair<bool, vector<int>> {
        int l = pipes[id].len;
        int ws = pipes[id].w_start, we = pipes[id].w_end;
        const uint64_t* m = pipes[id].mask;

        vector<int> conf_pipes;
        for (int x = s; x < s + l; ++x) {
            for (int p_other : pipes_at_x[x]) {
                for (int w = max(ws, pipes[p_other].w_start); w <= min(we, pipes[p_other].w_end); ++w) {
                    if (m[w] & pipes[p_other].mask[w]) {
                        if (p_other <= 4006) return {true, {}}; // prefix conflict
                        conf_pipes.push_back(p_other);
                        break;
                    }
                }
            }
        }
        sort(conf_pipes.begin(), conf_pipes.end());
        conf_pipes.erase(unique(conf_pipes.begin(), conf_pipes.end()), conf_pipes.end());
        return {false, conf_pipes};
    };

    int max_limit = 21800; // Let's try to get pipe 5048 below 21800!
    int target_p = 5048;
    cout << "Searching 2-ejection chain for pipe 5048 to get under " << max_limit << "..." << endl;

    int orig_s = pipes[target_p].shift;
    remove_pipe(target_p);

    bool solved = false;
    for (int s = 0; s + pipes[target_p].len <= max_limit; ++s) {
        auto [pref1, confs1] = get_conflicts(target_p, s);
        if (pref1) continue;
        if (confs1.empty()) {
            add_pipe(target_p, s);
            cout << "Direct fit found at " << s << endl;
            solved = true;
            break;
        }
        if (confs1.size() == 1) {
            int q1 = confs1[0];
            int s_q1 = pipes[q1].shift;
            remove_pipe(q1);
            add_pipe(target_p, s);

            // Level 1: can q1 fit directly?
            int fit_q1 = find_fit(q1, max_limit);
            if (fit_q1 != -1) {
                add_pipe(q1, fit_q1);
                cout << "1-Ejection found: 5048 -> " << s << ", " << q1 << " -> " << fit_q1 << endl;
                solved = true;
                break;
            }

            // Level 2: can q1 eject q2?
            for (int s2 = 0; s2 + pipes[q1].len <= max_limit; ++s2) {
                auto [pref2, confs2] = get_conflicts(q1, s2);
                if (pref2) continue;
                if (confs2.size() == 1 && confs2[0] != target_p) {
                    int q2 = confs2[0];
                    int s_q2 = pipes[q2].shift;
                    remove_pipe(q2);
                    add_pipe(q1, s2);

                    int fit_q2 = find_fit(q2, max_limit);
                    if (fit_q2 != -1) {
                        add_pipe(q2, fit_q2);
                        cout << "2-EJECTION SUCCESS! " << target_p << " -> " << s 
                             << ", " << q1 << " -> " << s2 
                             << ", " << q2 << " -> " << fit_q2 << endl;
                        solved = true;
                        break;
                    }
                    // Undo level 2
                    remove_pipe(q1);
                    add_pipe(q2, s_q2);
                }
            }

            if (solved) break;

            // Undo level 1
            remove_pipe(target_p);
            add_pipe(q1, s_q1);
        }
    }

    if (!solved) {
        add_pipe(target_p, orig_s);
        cout << "2-ejection failed for pipe 5048." << endl;
    }
    return 0;
}
