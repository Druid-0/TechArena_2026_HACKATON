
#include <iostream>
#include <vector>
#include <fstream>
#include <map>
#include <set>
#include <cstdint>

using namespace std;

int main() {
    ifstream in("tests/15.in");
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

    auto run_sim = [&](int r_low, int r_mid, int r_high, int split1, int split2) -> int {
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

        vector<pair<int, int>> pipes(130256);
        int pid = 1;

        for (int step = 0; step < n; ++step) {
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
                        int min_rem = 14;
                        if (pid >= 4007) {
                            if (req <= split1) min_rem = r_low;
                            else if (req <= split2) min_rem = r_mid;
                            else min_rem = r_high;
                        }
                        if (it->first - req >= min_rem || it->first == req) {
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
                    pipes[pid] = {place_shift, req};
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
                    pipes[pid] = {trailing_s, req};
                }
                pid++;
            } else {
                int id = -op;
                int s = pipes[id].first;
                int l = pipes[id].second;

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
        return trench;
    };

    cout << "Baseline (8, 8, 16, 12, 12): " << run_sim(8, 8, 16, 12, 12) << endl;

    int best = 431660;
    for (int r_low : {6, 7, 8, 9, 10}) {
        for (int r_mid : {10, 12, 14, 16}) {
            for (int r_high : {16, 18, 20}) {
                for (int s1 : {6, 8, 10, 12}) {
                    for (int s2 : {16, 20, 24}) {
                        int tr = run_sim(r_low, r_mid, r_high, s1, s2);
                        if (tr < best) {
                            best = tr;
                            cout << "NEW BEST: " << tr << " (low=" << r_low 
                                 << ", mid=" << r_mid << ", high=" << r_high 
                                 << ", s1=" << s1 << ", s2=" << s2 << ")" 
                                 << " Score=" << (double)418757 / tr * 10.0 << endl;
                        }
                    }
                }
            }
        }
    }
    cout << "Done. Best: " << best << endl;
    return 0;
}
