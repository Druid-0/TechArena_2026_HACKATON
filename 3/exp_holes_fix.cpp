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

    // Experiment with fallback strategies:
    // 0: fallback to best-fit (current)
    // 1: fallback to worst-fit
    // 2: fallback to trailing (dig at end)
    for (int fb_mode : {0, 1, 2}) {
        for (int param : {10, 14, 16, 20}) {
            map<int, int> by_start;
            set<pair<int, int>> by_len;
            vector<pair<int, int>> pipes;
            pipes.emplace_back(-1, -1);
            int trench_len = 0;
            int occupied = 0;

            auto add_free = [&](int start, int len) {
                if (len <= 0) return;
                by_start[start] = len;
                by_len.insert({len, start});
            };

            auto remove_free = [&](int start, int len) {
                by_start.erase(start);
                by_len.erase({len, start});
            };

            for (int op : ops) {
                if (op == 0) break;
                if (op > 0) {
                    int req_len = op;
                    occupied += req_len;
                    int best_start = -1, block_len = -1;

                    // 1. Try exact match first
                    auto it_exact = by_len.lower_bound({req_len, -1});
                    if (it_exact != by_len.end() && it_exact->first == req_len) {
                        best_start = it_exact->second;
                        block_len = it_exact->first;
                    } else {
                        // 2. Try to find block with remainder >= param
                        auto it = by_len.lower_bound({req_len + param, -1});
                        if (it != by_len.end()) {
                            best_start = it->second;
                            block_len = it->first;
                        } else {
                            // Fallback
                            if (fb_mode == 0) { // best fit
                                auto it_bf = by_len.lower_bound({req_len, -1});
                                if (it_bf != by_len.end()) {
                                    best_start = it_bf->second;
                                    block_len = it_bf->first;
                                }
                            } else if (fb_mode == 1) { // worst fit
                                if (!by_len.empty() && prev(by_len.end())->first >= req_len) {
                                    best_start = prev(by_len.end())->second;
                                    block_len = prev(by_len.end())->first;
                                }
                            } // fb_mode == 2: leave best_start = -1 (extend trench)
                        }
                    }

                    if (best_start != -1) {
                        remove_free(best_start, block_len);
                        int shift = best_start;
                        int rem = block_len - req_len;
                        if (rem > 0) add_free(shift + req_len, rem);
                        pipes.emplace_back(shift, req_len);
                    } else {
                        int trailing_start = trench_len;
                        int trailing_len = 0;
                        if (!by_start.empty()) {
                            auto last_it = prev(by_start.end());
                            if (last_it->first + last_it->second == trench_len) {
                                trailing_start = last_it->first;
                                trailing_len = last_it->second;
                                remove_free(trailing_start, trailing_len);
                            }
                        }
                        int needed = req_len - trailing_len;
                        int cnt = (needed + 19) / 20;
                        trench_len += cnt * 20;
                        trailing_len += cnt * 20;

                        int shift = trailing_start;
                        int rem = trailing_len - req_len;
                        if (rem > 0) add_free(shift + req_len, rem);
                        pipes.emplace_back(shift, req_len);
                    }
                } else {
                    int id = -op;
                    occupied -= pipes[id].second;
                    int shift = pipes[id].first;
                    int len = pipes[id].second;
                    pipes[id] = {-1, -1};

                    auto it = by_start.lower_bound(shift);
                    int cur_start = shift;
                    int cur_len = len;

                    if (it != by_start.begin()) {
                        auto left = prev(it);
                        if (left->first + left->second == cur_start) {
                            cur_start = left->first;
                            cur_len += left->second;
                            remove_free(left->first, left->second);
                        }
                    }
                    auto right = by_start.lower_bound(cur_start + cur_len);
                    if (right != by_start.end() && right->first == cur_start + cur_len) {
                        cur_len += right->second;
                        remove_free(right->first, right->second);
                    }
                    add_free(cur_start, cur_len);
                }
            }
            double score = (double)occupied / trench_len * w;
            cout << "fb_mode=" << fb_mode << ", param=" << param 
                 << ": Trench=" << trench_len << ", Score=" << fixed << score << endl;
        }
    }
    return 0;
}
