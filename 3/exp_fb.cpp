
#include <iostream>
#include <vector>
#include <fstream>
#include <set>
#include <map>
#include <algorithm>
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

    // Test fallback modes when no hole with rem >= 14 is found:
    // 0: fallback to tightest (found_s) - current
    // 1: fallback to trailing (best_s remains -1)
    // 2: fallback to worst-fit (largest hole)
    // 3: search all holes without count limit for rem >= 14
    for (int fb_mode : {0, 1, 2, 3}) {
        map<int, int> by_start;
        set<pair<int, int>> by_len;
        vector<pair<int, int>> pipes;
        pipes.emplace_back(-1, -1);
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

        int pid = 1;
        for (int op : ops) {
            if (op == 0) break;
            if (op > 0) {
                int req = op;
                bool perm = is_perm(pid);
                int best_s = -1, best_l = -1;

                if (perm) {
                    auto it = by_len.lower_bound({req, -1});
                    if (it != by_len.end() && it->first == req) {
                        best_s = it->second; best_l = it->first;
                    }
                } else {
                    auto it = by_len.lower_bound({req, -1});
                    int found_s = -1, found_l = -1;
                    int count = 0;
                    int limit = (fb_mode == 3 ? 1000000 : 18);
                    for (; it != by_len.end() && count < limit; ++it, ++count) {
                        if (found_s == -1) { found_s = it->second; found_l = it->first; }
                        if (it->first - req >= 14 || it->first == req) {
                            best_s = it->second; best_l = it->first; break;
                        }
                    }
                    if (best_s == -1) {
                        if (fb_mode == 0) {
                            best_s = found_s; best_l = found_l;
                        } else if (fb_mode == 2) {
                            // worst-fit
                            if (!by_len.empty() && prev(by_len.end())->first >= req) {
                                auto w_it = prev(by_len.end());
                                best_s = w_it->second; best_l = w_it->first;
                            }
                        }
                        // if fb_mode == 1, best_s remains -1 -> trailing
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
                    pipes.emplace_back(place_shift, req);
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
                    pipes.emplace_back(trailing_s, req);
                }
                pid++;
            } else {
                int id = -op;
                int s = pipes[id].first;
                int l = pipes[id].second;
                pipes[id] = {-1, -1};

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
        cout << "fb_mode=" << fb_mode << ": Trench=" << trench 
             << ", Score=" << (double)418757 / trench * 10.0 << endl;
    }
    return 0;
}
