#include <iostream>
#include <vector>
#include <fstream>
#include <map>
#include <set>
#include <algorithm>

using namespace std;

int main() {
    ifstream in("tests/15.in");
    double w;
    int n;
    in >> w >> n;
    vector<int> ops(n);
    for (int i = 0; i < n; ++i) in >> ops[i];
    
    cout << "Test 15 loaded: " << n << " operations." << endl;
    
    // Experiment with different threshold strategies:
    // e.g. when allocating, prioritize fitting small pipes into small holes
    // or try different remainder thresholds
    for (int rem_thresh : {0, 5, 10, 15, 20, 25, 30, 35, 40}) {
        map<int, int> by_start;
        map<int, set<int>> by_len;
        vector<pair<int, int>> pipes;
        pipes.emplace_back(-1, -1);
        
        int trench = 0;
        int occupied = 0;
        
        auto add_free = [&](int start, int len) {
            if (len <= 0) return;
            by_start[start] = len;
            by_len[len].insert(start);
        };
        
        auto remove_free = [&](int start, int len) {
            by_start.erase(start);
            auto it = by_len.find(len);
            if (it != by_len.end()) {
                it->second.erase(start);
                if (it->second.empty()) by_len.erase(it);
            }
        };
        
        int pid = 1;
        for (int op : ops) {
            if (op == 0) break;
            if (op > 0) {
                int req = op;
                occupied += req;
                int best_start = -1;
                int block_len = -1;
                
                // Strategy: Exact match first
                auto it = by_len.find(req);
                if (it != by_len.end()) {
                    block_len = it->first;
                    best_start = *it->second.begin();
                } else {
                    // Try to find a block where remainder >= rem_thresh or remainder == 0
                    if (rem_thresh > 0) {
                        auto b_it = by_len.lower_bound(req);
                        int fallback_s = -1, fallback_l = -1;
                        int count = 0;
                        for (; b_it != by_len.end() && count < 20; ++b_it, ++count) {
                            if (fallback_s == -1) {
                                fallback_s = *b_it->second.begin();
                                fallback_l = b_it->first;
                            }
                            if (b_it->first - req >= rem_thresh) {
                                best_start = *b_it->second.begin();
                                block_len = b_it->first;
                                break;
                            }
                        }
                        if (best_start == -1 && fallback_s != -1) {
                            best_start = fallback_s;
                            block_len = fallback_l;
                        }
                    } else {
                        // Standard policy 4: worst-fit if no exact match
                        if (!by_len.empty() && prev(by_len.end())->first >= req) {
                            auto w_it = prev(by_len.end());
                            block_len = w_it->first;
                            best_start = *w_it->second.begin();
                        }
                    }
                }
                
                if (best_start != -1) {
                    remove_free(best_start, block_len);
                    int shift = best_start;
                    int rem = block_len - req;
                    if (rem > 0) add_free(shift + req, rem);
                    pipes.emplace_back(shift, req);
                } else {
                    int trailing_start = trench;
                    int trailing_len = 0;
                    if (!by_start.empty()) {
                        auto last_it = prev(by_start.end());
                        if (last_it->first + last_it->second == trench) {
                            trailing_start = last_it->first;
                            trailing_len = last_it->second;
                            remove_free(trailing_start, trailing_len);
                        }
                    }
                    int needed = req - trailing_len;
                    int cnt = (needed + 19) / 20;
                    trench += cnt * 20;
                    trailing_len += cnt * 20;
                    int shift = trailing_start;
                    int rem = trailing_len - req;
                    if (rem > 0) add_free(shift + req, rem);
                    pipes.emplace_back(shift, req);
                }
                pid++;
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
        double score = (double)occupied / trench * w;
        cout << "rem_thresh=" << rem_thresh << ": Trench=" << trench << ", Score=" << fixed << score << endl;
    }
    return 0;
}
