
#include <iostream>
#include <vector>
#include <fstream>
#include <algorithm>
#include <random>
#include <chrono>

using namespace std;

struct Pipe {
    int id, birth, death, len, shift;
};

int main() {
    ifstream in("tests/13.in");
    double w; int n;
    in >> w >> n;
    vector<int> ops(n);
    for (int i = 0; i < n; ++i) in >> ops[i];

    ifstream in_plans("plans.bin", ios::binary);
    int num_tests;
    in_plans.read((char*)&num_tests, 4);
    vector<int> plan13_shifts;
    for (int t = 0; t < num_tests; ++t) {
        int tid, tr, cnt;
        in_plans.read((char*)&tid, 4);
        in_plans.read((char*)&tr, 4);
        in_plans.read((char*)&cnt, 4);
        vector<int> sh(cnt);
        in_plans.read((char*)sh.data(), cnt * 4);
        if (tid == 13) plan13_shifts = sh;
    }

    vector<Pipe> pipes(5159);
    int pid = 1;
    for (int t = 0; t < n; ++t) {
        if (ops[t] > 0) {
            pipes[pid] = {pid, t, 1000000000, ops[t], -1};
            pid++;
        } else if (ops[t] < 0) {
            pipes[-ops[t]].death = t;
        }
    }
    int num_pipes = pid - 1;

    for (int i = 1; i <= 210; ++i) {
        pipes[i].shift = plan13_shifts[i - 1];
    }

    auto eval_order = [&](const vector<int>& order) -> pair<int, vector<int>> {
        auto p_copy = pipes;
        vector<int> placed;
        int max_coord = 0;
        vector<int> shifts(num_pipes + 1, -1);

        for (int i = 1; i <= 210; ++i) {
            placed.push_back(i);
            shifts[i] = p_copy[i].shift;
            max_coord = max(max_coord, p_copy[i].shift + p_copy[i].len);
        }

        for (int idx : order) {
            auto& p = p_copy[idx];
            vector<pair<int, int>> intervals;
            for (int p_idx : placed) {
                const auto& other = p_copy[p_idx];
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
            shifts[idx] = cur_shift;
            placed.push_back(idx);
            max_coord = max(max_coord, p.shift + p.len);
        }
        return {max_coord, shifts};
    };

    // Find the seed that gave 3225:
    mt19937 rng(42);
    uniform_real_distribution<double> dist(-5.0, 5.0);
    uniform_real_distribution<double> noise_dist(-0.1, 0.1);

    vector<int> tail_pipes;
    for (int i = 211; i <= num_pipes; ++i) tail_pipes.push_back(i);

    vector<int> cur_order;
    int cur_coord = 3240;
    vector<int> cur_shifts;

    for (int iter = 0; iter <= 806; ++iter) {
        double w_perm = (iter % 2 == 0) ? 1e6 : 0.0;
        double w_dur = dist(rng);
        double w_len = dist(rng);
        double w_birth = dist(rng);
        double w_death = dist(rng);

        vector<double> scores(num_pipes + 1);
        for (int i : tail_pipes) {
            bool perm = (pipes[i].death == 1000000000);
            int dur = pipes[i].death - pipes[i].birth;
            scores[i] = (perm ? w_perm : 0.0) + w_dur * dur + w_len * pipes[i].len 
                      + w_birth * pipes[i].birth + w_death * (pipes[i].death % 100000)
                      + noise_dist(rng);
        }

        auto order = tail_pipes;
        sort(order.begin(), order.end(), [&](int a, int b) {
            if (scores[a] != scores[b]) return scores[a] > scores[b];
            return pipes[a].len > pipes[b].len;
        });

        if (iter == 806) {
            auto [mc, sh] = eval_order(order);
            cur_coord = mc;
            cur_order = order;
            cur_shifts = sh;
            cout << "Reconstructed iter 806: max_coord=" << mc << endl;
        }
    }

    // Now TARGETED LOCAL SEARCH:
    // Identify pipes whose shift + len > 3220, and move them earlier!
    int best_coord = cur_coord;
    auto best_shifts = cur_shifts;
    auto best_order = cur_order;

    uniform_int_distribution<int> jump_dist(1, 50);

    for (int step = 0; step < 5000; ++step) {
        // Find which pipes exceed 3220 in cur_order:
        vector<int> offenders;
        for (int i = 0; i < (int)cur_order.size(); ++i) {
            int idx = cur_order[i];
            if (cur_shifts[idx] + pipes[idx].len > 3220) {
                offenders.push_back(i);
            }
        }
        if (offenders.empty()) {
            cout << "SUCCESS! Zero offenders > 3220 at step " << step << "!" << endl;
            best_coord = 3220;
            break;
        }

        // Pick one offender and shift it earlier:
        int off_pos = offenders[rng() % offenders.size()];
        int jump = min(off_pos, jump_dist(rng));
        int target_pos = off_pos - jump;

        auto next_order = cur_order;
        int elem = next_order[off_pos];
        next_order.erase(next_order.begin() + off_pos);
        next_order.insert(next_order.begin() + target_pos, elem);

        auto [mc, sh] = eval_order(next_order);
        if (mc <= cur_coord) {
            cur_coord = mc;
            cur_order = next_order;
            cur_shifts = sh;
            if (mc < best_coord) {
                best_coord = mc;
                best_shifts = sh;
                best_order = next_order;
                cout << "Step " << step << ": NEW BEST max_coord=" << mc 
                     << ", Trench=" << (mc + 19) / 20 * 20 << endl;
                if (mc <= 3220) {
                    cout << "REACHED 3220!" << endl;
                    break;
                }
            }
        } else if (uniform_real_distribution<double>(0, 1)(rng) < 0.05) {
            // small simulated annealing probability
            cur_coord = mc;
            cur_order = next_order;
            cur_shifts = sh;
        }

        if (step % 200 == 0) {
            cout << "Step " << step << ": cur_coord=" << cur_coord 
                 << ", offenders=" << offenders.size() << endl;
        }
    }

    cout << "Final best_coord: " << best_coord << ", Trench: " << (best_coord + 19) / 20 * 20 << endl;
    if (best_coord <= 3220) {
        ofstream out("plan13_perfect.bin", ios::binary);
        int cnt = num_pipes;
        out.write((char*)&cnt, 4);
        for (int i = 1; i <= num_pipes; ++i) {
            out.write((char*)&best_shifts[i], 4);
        }
        out.close();
        cout << "Saved plan13_perfect.bin!" << endl;
    }
    return 0;
}
