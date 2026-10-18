#include <iostream>
#include <vector>
#include <fstream>
#include <algorithm>
#include <chrono>

using namespace std;

struct Pipe {
    int id, birth, death, len, shift;
};

int main() {
    auto t0 = chrono::high_resolution_clock::now();
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
    cout << "Loaded " << num_pipes << " pipes in " 
         << chrono::duration<double>(chrono::high_resolution_clock::now() - t0).count() << "s." << endl;

    // Fast interval packing:
    // We want to find for each pipe an interval [shift, shift + len)
    // We can maintain a grid or interval set.
    // Notice that max trench is around 420,000.
    // For each position x, what is the latest time it becomes permanently occupied,
    // or what intervals are occupied at x?
    // Since len <= 40, let's test how fast we can check:
    
    // Idea: Segment tree over positions 0 .. 450,000
    // Each position x has a list of [birth, death) intervals.
    // But since permanent pipes (death = 10^9) never die, once a permanent pipe is placed at x,
    // position x is dead from [p.birth, 10^9).
    
    return 0;
}
