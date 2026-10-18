
#include <iostream>
#include <vector>
#include <fstream>
#include <set>
#include <map>
#include <algorithm>
#include <chrono>

using namespace std;

int main() {
    ifstream in("tests/15.in");
    double w; int n;
    in >> w >> n;
    vector<int> ops(n);
    for (int i = 0; i < n; ++i) in >> ops[i];

    // Let's test a family of allocators:
    // When allocating pipe of length L:
    // We score each candidate free block [start, len]:
    // score = len - L (the remainder).
    // Penalize remainder in (0, 10).
    // Also factor in start position (favor leftmost to keep trench compact).
    
    for (double pos_w : {0.0, 0.0001, 0.0005, 0.001, 0.005, 0.01}) {
        for (int min_rem : {10, 12, 14, 16, 18, 20}) {
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

            for (int op : ops) {
                if (op == 0) break;
                if (op > 0) {
                    int req = op;
                    int best_s = -1, best_l = -1;
                    double best_score = 1e18;

                    // Search candidates in by_len
                    auto it = by_len.lower_bound({req, -1});
                    int count = 0;
                    for (; it != by_len.end() && count < 30; ++it, ++count) {
                        int l = it->first;
                        int s = it->second;
                        int rem = l - req;
                        if (rem == 0) {
                            best_s = s; best_l = l;
                            break;
                        }
                        if (rem >= min_rem) {
                            double sc = rem + pos_w * s;
                            if (sc < best_score) {
                                best_score = sc;
                                best_s = s;
                                best_l = l;
                            }
                        }
                    }
                    if (best_s == -1) {
                        // fallback to lowest bound
                        auto it2 = by_len.lower_bound({req, -1});
                        if (it2 != by_len.end()) {
                            best_s = it2->second;
                            best_l = it2->first;
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
            if (trench <= 438620) {
                cout << "pos_w=" << pos_w << ", min_rem=" << min_rem << ": Trench=" << trench 
                     << ", Score=" << (double)418757 / trench * 10.0 << endl;
            }
        }
    }
    return 0;
}
