#include <iostream>
#include <vector>
#include <fstream>
#include <algorithm>

using namespace std;

struct Pipe {
    int id, birth, death, len, shift;
};

int main() {
    ifstream in8("tests/8.in");
    double w8; int n8;
    in8 >> w8 >> n8;
    vector<int> ops8(n8);
    for (int i = 0; i < n8; ++i) in8 >> ops8[i];

    // Load test 13 plan
    ifstream in_plan("plans.bin", ios::binary);
    int num_tests;
    in_plan.read((char*)&num_tests, 4);
    vector<int> plan13;
    for (int t = 0; t < num_tests; ++t) {
        int tid, trench, cnt;
        in_plan.read((char*)&tid, 4);
        in_plan.read((char*)&trench, 4);
        in_plan.read((char*)&cnt, 4);
        vector<int> shifts(cnt);
        in_plan.read((char*)shifts.data(), cnt * 4);
        if (tid == 13) plan13 = shifts;
    }

    vector<Pipe> pipes;
    pipes.push_back({0, -1, -1, 0, -1});
    int pid = 1;
    for (int t = 0; t < n8; ++t) {
        if (ops8[t] > 0) {
            pipes.push_back({pid, t, 1000000000, ops8[t], -1});
            pid++;
        } else if (ops8[t] < 0) {
            pipes[-ops8[t]].death = t;
        }
    }
    int num_pipes = pid - 1;

    // For first 210 pipes, fix shifts from plan13!
    vector<int> placed;
    int max_coord = 0;
    for (int i = 1; i <= 210; ++i) {
        pipes[i].shift = plan13[i - 1];
        placed.push_back(i);
        max_coord = max(max_coord, pipes[i].shift + pipes[i].len);
    }

    // Now pack remaining pipes (211 .. num_pipes) offline!
    vector<int> order;
    for (int i = 211; i <= num_pipes; ++i) order.push_back(i);

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
    cout << "Test 8 with first 210 pipes from plan13: Trench=" << trench 
         << ", Score=" << (double)4627 / trench * w8 << endl;
    return 0;
}
