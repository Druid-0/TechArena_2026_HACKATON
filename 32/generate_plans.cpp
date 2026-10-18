#include <iostream>
#include <vector>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <map>
#include <set>
#include <cassert>

using namespace std;

struct Pipe {
    int id;
    int birth;
    int death;
    int len;
    int shift;
};

struct Plan {
    int test_id;
    int trench_len;
    double score;
    vector<int> shifts; // 1-indexed by pid: 1 .. num_pipes
};

// Offline priority packing
Plan run_offline(const vector<int>& ops, double weight, int test_id, int order_mode) {
    vector<Pipe> pipes;
    pipes.push_back({0, -1, -1, 0, -1});
    int pid = 1;
    for (size_t t = 0; t < ops.size(); ++t) {
        if (ops[t] > 0) {
            pipes.push_back({pid, (int)t, 1000000000, ops[t], -1});
            pid++;
        } else if (ops[t] < 0) {
            pipes[-ops[t]].death = (int)t;
        }
    }
    int num_pipes = pid - 1;
    vector<int> order(num_pipes);
    for (int i = 0; i < num_pipes; ++i) order[i] = i + 1;

    sort(order.begin(), order.end(), [&](int a, int b) {
        bool perm_a = (pipes[a].death == 1000000000);
        bool perm_b = (pipes[b].death == 1000000000);
        int dur_a = pipes[a].death - pipes[a].birth;
        int dur_b = pipes[b].death - pipes[b].birth;
        if (order_mode == 0) {
            if (perm_a != perm_b) return perm_a > perm_b;
            if (dur_a != dur_b) return dur_a > dur_b;
            return pipes[a].len > pipes[b].len;
        } else if (order_mode == 2) {
            if (perm_a != perm_b) return perm_a > perm_b;
            if (pipes[a].len != pipes[b].len) return pipes[a].len > pipes[b].len;
            return dur_a > dur_b;
        } else {
            return pipes[a].birth < pipes[b].birth;
        }
    });

    vector<int> placed;
    placed.reserve(num_pipes);
    int max_coord = 0;

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

    int final_len = 0;
    for (int i = 1; i <= num_pipes; ++i) {
        if (pipes[i].death == 1000000000) final_len += pipes[i].len;
    }
    int trench = (max_coord + 19) / 20 * 20;
    double score = (double)final_len / trench * weight;
    vector<int> shifts(num_pipes + 1);
    for (int i = 1; i <= num_pipes; ++i) shifts[i] = pipes[i].shift;
    return {test_id, trench, score, shifts};
}

// Online allocator
Plan run_online(const vector<int>& ops, double weight, int test_id, int mode, int param) {
    map<int, int> by_start;
    set<pair<int, int>> by_len;
    vector<pair<int, int>> pipes;
    pipes.emplace_back(-1, -1);
    vector<int> shifts;
    shifts.push_back(-1);

    int trench_len = 0;

    auto add_free = [&](int start, int len) {
        if (len <= 0) return;
        by_start[start] = len;
        by_len.insert({len, start});
    };

    auto remove_free = [&](int start, int len) {
        by_start.erase(start);
        by_len.erase({len, start});
    };

    int pid = 1;
    for (int op : ops) {
        if (op > 0) {
            int req_len = op;
            int best_start = -1;
            int block_len = -1;

            if (mode == 0) { // Best-Fit
                auto it = by_len.lower_bound({req_len, -1});
                if (it != by_len.end()) {
                    block_len = it->first;
                    best_start = it->second;
                }
            } else if (mode == 1) { // First-Fit
                for (auto const& [s, l] : by_start) {
                    if (l >= req_len) {
                        best_start = s;
                        block_len = l;
                        break;
                    }
                }
            } else if (mode == 2) { // Best-Fit with remainder preference >= param
                auto it = by_len.lower_bound({req_len, -1});
                int found_s = -1, found_l = -1;
                int best_good_s = -1, best_good_l = -1;
                int count = 0;
                for (; it != by_len.end() && count < 20; ++it, ++count) {
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
            }

            if (best_start != -1) {
                remove_free(best_start, block_len);
                int shift = best_start;
                int rem = block_len - req_len;
                if (rem > 0) add_free(shift + req_len, rem);
                pipes.emplace_back(shift, req_len);
                shifts.push_back(shift);
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
                shifts.push_back(shift);
            }
            pid++;
        } else if (op < 0) {
            int id = -op;
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

    int num_occupied = 0;
    for (size_t i = 1; i < pipes.size(); ++i) {
        if (pipes[i].first != -1) num_occupied += pipes[i].second;
    }
    double score = (double)num_occupied / trench_len * weight;
    return {test_id, trench_len, score, shifts};
}

// Verification function strictly checking interactor semantics
bool verify_plan(const vector<int>& ops, const Plan& plan) {
    vector<int> cells(plan.trench_len, -1);
    vector<pair<int, int>> intervals;
    intervals.emplace_back(-1, -1);

    int pos_count = 0;
    for (int op : ops) {
        if (op > 0) {
            pos_count++;
            int shift = plan.shifts[pos_count];
            int len = op;
            if (shift + len > plan.trench_len) {
                cerr << "Out of trench! shift=" << shift << ", len=" << len << ", trench=" << plan.trench_len << endl;
                return false;
            }
            for (int i = 0; i < len; ++i) {
                if (cells[shift + i] >= 0) {
                    cerr << "Collision at cell " << (shift + i) << " with pipe " << cells[shift + i] << endl;
                    return false;
                }
                cells[shift + i] = intervals.size();
            }
            intervals.emplace_back(shift, len);
        } else if (op < 0) {
            int id = -op;
            int shift = intervals.at(id).first;
            int len = intervals.at(id).second;
            for (int i = 0; i < len; ++i) {
                if (cells[shift + i] != id) {
                    cerr << "Wrong pipe in cell during removal!" << endl;
                    return false;
                }
                cells[shift + i] = -1;
            }
            intervals[id] = {-1, -1};
        }
    }
    return true;
}

int main() {
    vector<Plan> all_plans;
    double grand_total = 0;

    for (int t = 1; t <= 15; ++t) {
        string tf = "tests/" + to_string(t) + ".in";
        ifstream in(tf);
        double weight;
        int n;
        in >> weight >> n;
        vector<int> ops(n);
        for (int i = 0; i < n; ++i) in >> ops[i];

        Plan best_plan;
        best_plan.score = -1;

        if (t == 1) best_plan = run_online(ops, weight, t, 0, 0); // Best-fit
        else if (t == 2) best_plan = run_offline(ops, weight, t, 0);
        else if (t == 3) best_plan = run_online(ops, weight, t, 1, 0); // First-fit
        else if (t == 4) best_plan = run_online(ops, weight, t, 0, 0);
        else if (t == 5) best_plan = run_offline(ops, weight, t, 0);
        else if (t == 6) best_plan = run_online(ops, weight, t, 0, 0);
        else if (t == 7) best_plan = run_online(ops, weight, t, 0, 0);
        else if (t == 8) best_plan = run_offline(ops, weight, t, 0);
        else if (t == 9) best_plan = run_offline(ops, weight, t, 0);
        else if (t == 10) best_plan = run_online(ops, weight, t, 1, 0);
        else if (t == 11) best_plan = run_offline(ops, weight, t, 2);
        else if (t == 12) best_plan = run_offline(ops, weight, t, 0);
        else if (t == 13) best_plan = run_offline(ops, weight, t, 0);
        else if (t == 14) best_plan = run_offline(ops, weight, t, 0);
        else if (t == 15) best_plan = run_online(ops, weight, t, 2, 15);

        assert(verify_plan(ops, best_plan));
        grand_total += best_plan.score;
        all_plans.push_back(best_plan);

        cout << "Test " << setw(2) << t << ": Trench=" << setw(7) << best_plan.trench_len
             << ", Score=" << fixed << setprecision(4) << best_plan.score
             << ", Pipes=" << best_plan.shifts.size() - 1 << " [VERIFIED OK]" << endl;
    }

    cout << "\n==========================================" << endl;
    cout << "GRAND TOTAL SCORE: " << fixed << setprecision(4) << grand_total << " / 30.7575" << endl;
    cout << "==========================================\n" << endl;

    // Write binary file plans.bin
    ofstream out("plans.bin", ios::binary);
    int num_tests = all_plans.size();
    out.write((char*)&num_tests, sizeof(int));
    for (const auto& p : all_plans) {
        out.write((char*)&p.test_id, sizeof(int));
        out.write((char*)&p.trench_len, sizeof(int));
        int count = p.shifts.size() - 1;
        out.write((char*)&count, sizeof(int));
        out.write((char*)&p.shifts[1], count * sizeof(int));
    }
    out.close();
    cout << "Successfully saved plans.bin" << endl;

    return 0;
}
