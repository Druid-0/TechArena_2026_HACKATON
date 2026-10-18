#include <iostream>
#include <vector>
#include <fstream>
#include <set>
#include <map>
#include <algorithm>

using namespace std;

int main() {
    ifstream in("tests/11.in");
    double w;
    int n;
    in >> w >> n;
    vector<int> ops(n);
    for (int i = 0; i < n; ++i) in >> ops[i];

    for (int param : {10, 12, 14, 15, 16, 20}) {
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
                int best_start = -1;
                int block_len = -1;

                auto it = by_len.lower_bound({req_len, -1});
                int found_s = -1, found_l = -1;
                int best_good_s = -1, best_good_l = -1;
                int count = 0;
                for (; it != by_len.end() && count < 22; ++it, ++count) {
                    if (found_s == -1) {
                        found_s = it->second;
                        found_l = it->first;
                    }
                    if (it->first - req_len >= param || it->first == req_len) {
                        best_good_s = it->second;
                        best_good_l = it->first;
                        break;
                    }
                }
                if (best_good_s != -1) {
                    best_start = best_good_s;
                    block_len = best_good_l;
                } else if (found_s != -1) {
                    best_start = found_s;
                    block_len = found_l;
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
        cout << "param=" << param << ": Trench=" << trench_len 
             << ", Score=" << fixed << score << endl;
    }
    return 0;
}
