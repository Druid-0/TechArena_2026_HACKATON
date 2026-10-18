#include <iostream>
#include <vector>
#include <fstream>
#include <algorithm>
#include <chrono>

using namespace std;

struct Pipe {
    int id;
    int birth;
    int death;
    int len;
    int shift;
};

int main() {
    auto t0 = chrono::high_resolution_clock::now();
    ifstream in("tests/15.in");
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
    cout << "Loaded " << num_pipes << " pipes." << endl;
    
    // Grid/bucket based interval tracking:
    // We have positions 0 .. 500,000.
    // For each position x, when is it occupied?
    // A position x has a list of occupied time intervals [birth, death).
    // To check if [cur_shift, cur_shift + len) is free in [birth, death):
    // We can maintain for each block of cells (e.g. per cell or per 20 cells)
    // the occupied intervals.
    // But since len <= 40, we only need to check len consecutive cells.
    
    // Instead of scanning all previous pipes (O(N^2)),
    // maintain for each cell x a list of [b, d) intervals.
    // Because cells are sorted by time, checking if a cell is free in [b, d)
    // is a binary search or two pointers!
    
    // Even faster:
    // Sort pipes: permanent pipes first!
    // Permanent pipes (death == 10^9) never die!
    // So all permanent pipes can be packed side-by-side with 0 gaps!
    // Length of all permanent pipes:
    int total_perm = 0;
    for (int i = 1; i <= num_pipes; ++i) {
        if (pipes[i].death == 1000000000) total_perm += pipes[i].len;
    }
    cout << "Total length of ALL permanent pipes: " << total_perm << endl;
    
    return 0;
}
