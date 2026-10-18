#include <iostream>
#include <vector>
#include <fstream>
#include <set>
#include <map>
#include <algorithm>

using namespace std;

struct Pipe {
    int id, birth, death, len, shift;
};

int main() {
    ifstream in("tests/11.in");
    double w;
    int n;
    in >> w >> n;
    vector<int> ops(n);
    for (int i = 0; i < n; ++i) in >> ops[i];

    vector<Pipe> pipes;
    pipes.push_back({0, -1, -1, 0, -1});
    int pid = 1;
    for (int t = 0; t < n; ++t) {
        if (ops[t] > 0) {
            pipes.push_back({pid, t, 1000000000, ops[t], -1});
            pid++;
        } else if (ops[t] < 0) {
            pipes[-ops[t]].death = t;
        }
    }
    int num_pipes = pid - 1;

    // Run online allocator for first 4999 ops:
    map<int, int> by_start;
    set<pair<int, int>> by_len;
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

    pid = 1;
    for (int t = 0; t < 4999; ++t) {
        int op = ops[t];
        if (op > 0) {
            int req_len = op;
            occupied += req_len;
            int best_start = -1, block_len = -1;

            auto it = by_len.lower_bound({req_len, -1});
            int found_s = -1, found_l = -1;
            int best_good_s = -1, best_good_l = -1;
            int count = 0;
            for (; it != by_len.end() && count < 22; ++it, ++count) {
                if (found_s == -1) {
                    found_s = it->second;
                    found_l = it->first;
                }
                if (it->first - req_len >= 14 || it->first == req_len) {
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
                pipes[pid].shift = shift;
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
                pipes[pid].shift = shift;
            }
            pid++;
        } else {
            int id = -op;
            occupied -= pipes[id].len;
            int shift = pipes[id].shift;
            int len = pipes[id].len;

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

    int online_pipes = pid - 1;
    cout << "Online phase finished: " << online_pipes << " pipes placed, trench=" << trench_len << endl;

    // Now pack remaining pipes (online_pipes + 1 .. num_pipes) offline!
    vector<int> placed;
    int max_coord = trench_len;
    for (int i = 1; i <= online_pipes; ++i) {
        placed.push_back(i);
    }

    vector<int> order;
    for (int i = online_pipes + 1; i <= num_pipes; ++i) order.push_back(i);

    sort(order.begin(), order.end(), [&](int a, int b) {
        bool perm_a = (pipes[a].death == 1000000000);
        bool perm_b = (pipes[b].death == 1000000000);
        if (perm_a != perm_b) return perm_a > perm_b;
        int dur_a = pipes[a].death - pipes[a].birth;
        int dur_b = pipes[b].death - pipes[b].birth;
        if (dur_a != dur_b) return dur_a > dur_b;
        return pipes[a].len > pipes[b].len;
    });

    for (int idx : order) {
        auto& p = pipes[idx];
        vector<pair<int, int>> intervals;
        for (int p_idx : placed) {
            const auto& other = pipes[p_idx];
            if (!(p.death <= other.birth || other.death <= p.birth)) {
                intervals.emplace_back(other.shift, other.shift + other.len);
            }
        }
        sort(intervals.begin(), intervals.end());

        int cur_shift = 0;
        for (const auto& iv : intervals) {
            if (cur_shift + p.len <= iv.first) break;
            if (cur_shift < iv.second) cur_shift = max(cur_shift, iv.second);
        }
        p.shift = cur_shift;
        placed.push_back(idx);
        max_coord = max(max_coord, p.shift + p.len);
    }

    int trench = (max_coord + 19) / 20 * 20;
    cout << "Test 11 hybrid: Trench=" << trench << ", Score=" << (double)20361 / trench * w << endl;
    return 0;
}
