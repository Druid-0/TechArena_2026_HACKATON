#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <algorithm>

using namespace std;

// Universal Worst-Fit allocator
// Note: pipe 1 has ALREADY been placed at shift = 0 with len = first_len, cnt = 1 (trench_len = 20)
void run_policy4_with_first_placed(int first_len) {
    map<int, int> by_start;
    set<pair<int, int>> by_len;
    vector<pair<int, int>> pipes;
    pipes.emplace_back(-1, -1);
    pipes.emplace_back(0, first_len); // pipe 1

    int trench_len = 20;
    if (20 - first_len > 0) {
        by_start[first_len] = 20 - first_len;
        by_len.insert({20 - first_len, first_len});
    }

    auto add_free = [&](int start, int len) {
        if (len <= 0) return;
        by_start[start] = len;
        by_len.insert({len, start});
    };

    auto remove_free = [&](int start, int len) {
        by_start.erase(start);
        by_len.erase({len, start});
    };

    int pid = 2;
    int instr;

    while (cin >> instr && instr != 0) {
        if (instr > 0) {
            int req_len = instr;
            int best_start = -1;
            int block_len = -1;

            auto it = by_len.lower_bound({req_len, -1});
            if (it != by_len.end() && it->first == req_len) {
                block_len = it->first;
                best_start = it->second;
            } else if (!by_len.empty() && prev(by_len.end())->first >= req_len) {
                auto w_it = prev(by_len.end());
                block_len = w_it->first;
                best_start = w_it->second;
            }

            int cnt = 0;
            int shift = -1;

            if (best_start != -1) {
                remove_free(best_start, block_len);
                shift = best_start;
                int rem = block_len - req_len;
                if (rem > 0) add_free(shift + req_len, rem);
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
                cnt = (needed + 19) / 20;
                trench_len += cnt * 20;
                trailing_len += cnt * 20;

                shift = trailing_start;
                int rem = trailing_len - req_len;
                if (rem > 0) add_free(shift + req_len, rem);
            }

            pipes.emplace_back(shift, req_len);
            cout << cnt << " " << shift << "\n";
            cout.flush();
            pid++;
        } else if (instr < 0) {
            int id = -instr;
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

            cout << shift << "\n";
            cout.flush();
        }
    }
}

int main() {
    return 0;
}
