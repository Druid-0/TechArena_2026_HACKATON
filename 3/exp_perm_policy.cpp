#include <cstdint>

#include <iostream>
#include <vector>
#include <fstream>
#include <set>
#include <map>
#include <algorithm>

using namespace std;

int main() {
    ifstream in("tests/15.in");
    double w; int n;
    in >> w >> n;
    vector<int> ops(n);
    for (int i = 0; i < n; ++i) in >> ops[i];

    // Load perm bits
    vector<uint8_t> perm_bits(16282);
    ifstream f_bits("perm_bits_15.bin", ios::binary);
    f_bits.read((char*)perm_bits.data(), 16282);

    auto is_perm = [&](int pid) -> bool {
        int idx = pid - 1;
        return (perm_bits[idx / 8] >> (idx % 8)) & 1;
    };

    // Test different policies for perm vs temp
    for (int perm_policy : {0, 1, 2, 3, 4}) {
        // perm_policy:
        // 0: exact match only for perm, else trailing
        // 1: exact match or rem >= 20 for perm, else trailing
        // 2: exact match or rem >= 30 for perm, else trailing
        // 3: pure trailing for perm (perm never takes holes)
        // 4: standard best-fit for all
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

                if (perm_policy == 0 && perm) {
                    // Exact match only
                    auto it = by_len.lower_bound({req, -1});
                    if (it != by_len.end() && it->first == req) {
                        best_s = it->second; best_l = it->first;
                    }
                } else if (perm_policy == 1 && perm) {
                    // Exact match or large remainder
                    auto it = by_len.lower_bound({req, -1});
                    if (it != by_len.end() && it->first == req) {
                        best_s = it->second; best_l = it->first;
                    } else {
                        auto it2 = by_len.lower_bound({req + 20, -1});
                        if (it2 != by_len.end()) {
                            best_s = it2->second; best_l = it2->first;
                        }
                    }
                } else if (perm_policy == 2 && perm) {
                    auto it = by_len.lower_bound({req, -1});
                    if (it != by_len.end() && it->first == req) {
                        best_s = it->second; best_l = it->first;
                    } else {
                        auto it2 = by_len.lower_bound({req + 30, -1});
                        if (it2 != by_len.end()) {
                            best_s = it2->second; best_l = it2->first;
                        }
                    }
                } else if (perm_policy == 3 && perm) {
                    // Perm never takes holes, always trailing
                } else {
                    // Standard search
                    auto it = by_len.lower_bound({req, -1});
                    int found_s = -1, found_l = -1;
                    int count = 0;
                    for (; it != by_len.end() && count < 22; ++it, ++count) {
                        if (found_s == -1) { found_s = it->second; found_l = it->first; }
                        if (it->first - req >= 14 || it->first == req) {
                            best_s = it->second; best_l = it->first; break;
                        }
                    }
                    if (best_s == -1 && found_s != -1) {
                        best_s = found_s; best_l = found_l;
                    }
                }

                if (!perm && best_s == -1) {
                    // Temp pipe search
                    auto it = by_len.lower_bound({req, -1});
                    int found_s = -1, found_l = -1;
                    int count = 0;
                    for (; it != by_len.end() && count < 22; ++it, ++count) {
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
                    if (rem > 0) add_free(best_s + req, rem);
                    pipes.emplace_back(best_s, req);
                } else {
                    int trailing_s = trench;
                    int trailing_l = 0;
                    if (!by_start.empty()) {
                        auto last = prev(by_start.end());
                        if (last->first + last->second == trench) {
                            trailing_s = last->first;
                            trailing_l = last->second;
                            remove_free(trailing_s, trailing_l);
                        }
                    }
                    int needed = req - trailing_l;
                    int cnt = (needed + 19) / 20;
                    trench += cnt * 20;
                    trailing_l += cnt * 20;
                    int rem = trailing_l - req;
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
        cout << "perm_policy=" << perm_policy << ": Trench=" << trench 
             << ", Score=" << (double)418757 / trench * 10.0 << endl;
    }
    return 0;
}
