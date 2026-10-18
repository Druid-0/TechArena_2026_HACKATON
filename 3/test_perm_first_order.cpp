#include <iostream>
#include <vector>
#include <fstream>
#include <cstdint>
#include <chrono>
#include <algorithm>
#include <map>
#include <set>

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

    auto eval_order = [&](const vector<int>& order) -> pair<int, int> {
        int max_coord = trench;
        int max_perm_coord = 0;

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
                    if (p.death == 1000000000 && end_c > max_perm_coord) max_perm_coord = end_c;
                    break;
                }
                s = conflict_x + 1;
            }
        }

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

        return {max_coord, max_perm_coord};
    };

    vector<int> perm_tail, temp_tail;
    for (int i = online_pipes + 1; i <= num_pipes; ++i) {
        if (pipes[i].death == 1000000000) perm_tail.push_back(i);
        else temp_tail.push_back(i);
    }

    cout << "Perm tail pipes: " << perm_tail.size() << ", Temp tail pipes: " << temp_tail.size() << endl;

    // Test different sortings for perm tail pipes:
    // 1. By birth ascending
    sort(perm_tail.begin(), perm_tail.end(), [&](int a, int b) {
        if (pipes[a].birth != pipes[b].birth) return pipes[a].birth < pipes[b].birth;
        return pipes[a].len > pipes[b].len;
    });

    // Test different sortings for temp tail pipes:
    // By birth ascending
    sort(temp_tail.begin(), temp_tail.end(), [&](int a, int b) {
        if (pipes[a].birth != pipes[b].birth) return pipes[a].birth < pipes[b].birth;
        return pipes[a].len > pipes[b].len;
    });

    vector<int> order = perm_tail;
    order.insert(order.end(), temp_tail.begin(), temp_tail.end());

    auto [mc, mpc] = eval_order(order);
    cout << "Perm-first (perm by birth asc, temp by birth asc):" << endl;
    cout << "  max_coord=" << mc << " (Trench=" << (mc + 19) / 20 * 20 << ")" << endl;
    cout << "  max_perm_coord=" << mpc << endl;

    // 2. Perm by length desc, temp by duration desc
    sort(perm_tail.begin(), perm_tail.end(), [&](int a, int b) {
        if (pipes[a].len != pipes[b].len) return pipes[a].len > pipes[b].len;
        return pipes[a].birth < pipes[b].birth;
    });
    sort(temp_tail.begin(), temp_tail.end(), [&](int a, int b) {
        int da = pipes[a].death - pipes[a].birth;
        int db = pipes[b].death - pipes[b].birth;
        if (da != db) return da > db;
        return pipes[a].len > pipes[b].len;
    });
    order = perm_tail;
    order.insert(order.end(), temp_tail.begin(), temp_tail.end());
    auto [mc2, mpc2] = eval_order(order);
    cout << "Perm-first (perm by len desc, temp by dur desc):" << endl;
    cout << "  max_coord=" << mc2 << " (Trench=" << (mc2 + 19) / 20 * 20 << ")" << endl;
    cout << "  max_perm_coord=" << mpc2 << endl;

    return 0;
}
