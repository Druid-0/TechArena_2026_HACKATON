#include <iostream>
#include <vector>
#include <fstream>
#include <map>
#include <set>
#include <algorithm>
#include <chrono>

using namespace std;

struct TestData {
    int id;
    double weight;
    vector<int> ops;
};

TestData load_test(int id) {
    ifstream in("tests/" + to_string(id) + ".in");
    double w;
    int n;
    in >> w >> n;
    vector<int> ops(n);
    for (int i = 0; i < n; ++i) in >> ops[i];
    return {id, w, ops};
}

struct AllocResult {
    int trench;
    int occupied;
    double score;
};

// Configurable online allocator
// mode:
// 0: Pure Best-Fit
// 1: Pure Worst-Fit
// 2: Exact Match, then Worst-Fit (Policy 4)
// 3: Exact Match, then Best-Fit
// 4: Best-Fit with remainder preference >= param
// 5: Worst-Fit but avoid creating remainder in [1..param]
// 6: Exact Match, then Best-Fit with remainder >= param
AllocResult run_sim(const TestData& td, int mode, int param, int tie_break) {
    // tie_break: 0 = leftmost (min start), 1 = rightmost (max start)
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
    for (int op : td.ops) {
        if (op == 0) break;
        if (op > 0) {
            int req = op;
            occupied += req;
            int best_start = -1;
            int block_len = -1;
            
            if (mode == 0) { // pure Best-Fit
                auto it = by_len.lower_bound(req);
                if (it != by_len.end()) {
                    block_len = it->first;
                    best_start = (tie_break == 0) ? *it->second.begin() : *it->second.rbegin();
                }
            } else if (mode == 1) { // pure Worst-Fit
                if (!by_len.empty() && prev(by_len.end())->first >= req) {
                    auto it = prev(by_len.end());
                    block_len = it->first;
                    best_start = (tie_break == 0) ? *it->second.begin() : *it->second.rbegin();
                }
            } else if (mode == 2) { // Exact, then Worst-Fit
                auto it = by_len.find(req);
                if (it != by_len.end()) {
                    block_len = it->first;
                    best_start = (tie_break == 0) ? *it->second.begin() : *it->second.rbegin();
                } else if (!by_len.empty() && prev(by_len.end())->first >= req) {
                    auto w_it = prev(by_len.end());
                    block_len = w_it->first;
                    best_start = (tie_break == 0) ? *w_it->second.begin() : *w_it->second.rbegin();
                }
            } else if (mode == 3) { // Exact, then Best-Fit
                auto it = by_len.find(req);
                if (it != by_len.end()) {
                    block_len = it->first;
                    best_start = (tie_break == 0) ? *it->second.begin() : *it->second.rbegin();
                } else {
                    auto b_it = by_len.lower_bound(req);
                    if (b_it != by_len.end()) {
                        block_len = b_it->first;
                        best_start = (tie_break == 0) ? *b_it->second.begin() : *b_it->second.rbegin();
                    }
                }
            } else if (mode == 4 || mode == 6) { // Remainder preference
                // If mode 6, try exact match first
                if (mode == 6) {
                    auto it = by_len.find(req);
                    if (it != by_len.end()) {
                        block_len = it->first;
                        best_start = (tie_break == 0) ? *it->second.begin() : *it->second.rbegin();
                    }
                }
                if (best_start == -1) {
                    auto it = by_len.lower_bound(req);
                    int fallback_s = -1, fallback_l = -1;
                    int good_s = -1, good_l = -1;
                    int count = 0;
                    for (; it != by_len.end() && count < 50; ++it, ++count) {
                        int cur_l = it->first;
                        int cur_s = (tie_break == 0) ? *it->second.begin() : *it->second.rbegin();
                        if (fallback_s == -1) {
                            fallback_s = cur_s;
                            fallback_l = cur_l;
                        }
                        if (cur_l == req || cur_l - req >= param) {
                            good_s = cur_s;
                            good_l = cur_l;
                            break;
                        }
                    }
                    if (good_s != -1) {
                        best_start = good_s;
                        block_len = good_l;
                    } else if (fallback_s != -1) {
                        best_start = fallback_s;
                        block_len = fallback_l;
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
    double score = (double)occupied / trench * td.weight;
    return {trench, occupied, score};
}

int main() {
    for (int t : {8, 11, 12, 13, 14, 15}) {
        auto td = load_test(t);
        cout << "\n============================================\n";
        cout << "Testing Test " << t << " (weight " << td.weight << ")\n";
        cout << "============================================\n";
        
        vector<pair<string, AllocResult>> results;
        
        // Test modes
        results.push_back({"Pure Best-Fit (left)", run_sim(td, 0, 0, 0)});
        results.push_back({"Pure Best-Fit (right)", run_sim(td, 0, 0, 1)});
        results.push_back({"Pure Worst-Fit (left)", run_sim(td, 1, 0, 0)});
        results.push_back({"Exact + Worst-Fit (left, current)", run_sim(td, 2, 0, 0)});
        results.push_back({"Exact + Best-Fit (left)", run_sim(td, 3, 0, 0)});
        results.push_back({"Exact + Best-Fit (right)", run_sim(td, 3, 0, 1)});
        
        for (int p : {5, 10, 15, 20, 25, 30}) {
            results.push_back({"Best-Fit rem>=" + to_string(p) + " (left)", run_sim(td, 4, p, 0)});
            results.push_back({"Exact + Best-Fit rem>=" + to_string(p) + " (left)", run_sim(td, 6, p, 0)});
        }
        
        sort(results.begin(), results.end(), [](auto& a, auto& b) {
            return a.second.score > b.second.score;
        });
        
        cout << "Top 5 configurations for Test " << t << ":\n";
        for (int i = 0; i < min(5, (int)results.size()); ++i) {
            cout << "  " << i + 1 << ". " << results[i].first 
                 << ": Trench=" << results[i].second.trench 
                 << ", Score=" << fixed << results[i].second.score << "\n";
        }
    }
    return 0;
}
