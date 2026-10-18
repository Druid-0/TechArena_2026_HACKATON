#include <iostream>
#include <vector>
#include <fstream>
#include <algorithm>

using namespace std;

struct Pipe {
    int id, birth, death, len, shift;
};

int main() {
    ifstream in("tests/15.in");
    double w; int n;
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

    // 1. Pack permanent pipes first:
    // Try sorting permanent pipes by birth ascending or birth descending or length
    // If sorted by birth ascending:
    // Earlier permanent pipes are on the left, so their b_x is small on the left, large on the right!
    // If sorted by birth descending:
    // Later permanent pipes are on the left, so their b_x is VERY LARGE on the left!
    // If b_x is large on the left, LOTS of temporary pipes can fit in [0, 418757) before b_x!
    
    for (int perm_order : {0, 1, 2}) {
        vector<int> perm_indices;
        for (int i = 1; i <= num_pipes; ++i) {
            if (pipes[i].death == 1000000000) perm_indices.push_back(i);
        }

        if (perm_order == 0) { // birth ascending
            sort(perm_indices.begin(), perm_indices.end(), [&](int a, int b) {
                return pipes[a].birth < pipes[b].birth;
            });
        } else if (perm_order == 1) { // birth descending
            sort(perm_indices.begin(), perm_indices.end(), [&](int a, int b) {
                return pipes[a].birth > pipes[b].birth;
            });
        } else if (perm_order == 2) { // length descending, then birth descending
            sort(perm_indices.begin(), perm_indices.end(), [&](int a, int b) {
                if (pipes[a].len != pipes[b].len) return pipes[a].len > pipes[b].len;
                return pipes[a].birth > pipes[b].birth;
            });
        }

        // Place permanent pipes
        int cur_x = 0;
        vector<int> perm_birth(420000, 0);
        for (int idx : perm_indices) {
            pipes[idx].shift = cur_x;
            for (int i = 0; i < pipes[idx].len; ++i) {
                perm_birth[cur_x + i] = pipes[idx].birth;
            }
            cur_x += pipes[idx].len;
        }

        // Now place temporary pipes:
        // Temporary pipes are placed in cells where they don't overlap with perm pipes or other temp pipes!
        // Maintain for each cell x a list of occupied intervals:
        // Initially, cell x has interval [perm_birth[x], 10^9).
        // Since temporary pipes are sorted by birth, can we place them?
        
        // Count how many temp pipes can fit inside [0, cur_x):
        vector<int> temp_indices;
        for (int i = 1; i <= num_pipes; ++i) {
            if (pipes[i].death != 1000000000) temp_indices.push_back(i);
        }
        
        // Check simple condition: how many temp pipes have d <= min(perm_birth) over an interval of length L?
        cout << "perm_order=" << perm_order << ": total perm length=" << cur_x << endl;
    }
    return 0;
}
