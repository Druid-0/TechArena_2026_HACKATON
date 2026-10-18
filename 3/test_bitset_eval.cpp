#include <iostream>
#include <vector>
#include <fstream>
#include <cstdint>
#include <chrono>
#include <algorithm>
#include <map>
#include <set>

using namespace std;

const int MAX_X = 22000;
const int TIME_WORDS = 80; // (10000 - 5000 + 63) / 64 = 79

uint64_t occupied[MAX_X][TIME_WORDS];

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

    // Build bitmasks for tail pipes (and alive prefix pipes)
    for (int i = 1; i <= num_pipes; ++i) {
        int t_start = max(4999, pipes[i].birth);
        int t_end = min(n, pipes[i].death); // exclusive
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

    // Run prefix simulation up to step 4999
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

    // Baseline prefix occupied bitset
    static uint64_t base_occupied[MAX_X][TIME_WORDS];
    for (int i = 1; i <= online_pipes; ++i) {
        if (pipes[i].death > 4999) {
            int s = pipes[i].shift;
            int l = pipes[i].len;
            for (int x = s; x < s + l; ++x) {
                for (int w = pipes[i].w_start; w <= pipes[i].w_end; ++w) {
                    base_occupied[x][w] |= pipes[i].mask[w];
                }
            }
        }
    }

    auto eval_order_fast = [&](const vector<int>& order) -> int {
        // Copy base_occupied
        for (int x = 0; x < MAX_X; ++x) {
            for (int w = 0; w < TIME_WORDS; ++w) {
                occupied[x][w] = base_occupied[x][w];
            }
        }

        int max_coord = trench;
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
                        if (occupied[x][w] & m[w]) {
                            conf = true;
                            break;
                        }
                    }
                    if (conf) {
                        conflict_x = x;
                        break;
                    }
                }
                if (conflict_x == -1) {
                    // Found valid s!
                    for (int x = s; x < s + l; ++x) {
                        for (int w = ws; w <= we; ++w) {
                            occupied[x][w] |= m[w];
                        }
                    }
                    if (s + l > max_coord) max_coord = s + l;
                    break;
                }
                s = conflict_x + 1;
            }
        }
        return (max_coord + 19) / 20 * 20;
    };

    vector<int> tail_pipes;
    for (int i = online_pipes + 1; i <= num_pipes; ++i) tail_pipes.push_back(i);

    auto t0 = chrono::high_resolution_clock::now();
    int reps = 100;
    int tr = 0;
    for (int r = 0; r < reps; ++r) {
        tr = eval_order_fast(tail_pipes);
    }
    auto t1 = chrono::high_resolution_clock::now();
    double total_ms = chrono::duration<double, milli>(t1 - t0).count();
    cout << "Bitset eval trench: " << tr << ", elapsed for " << reps << " evals: " << total_ms 
         << " ms (" << total_ms / reps << " ms/eval)" << endl;

    return 0;
}
