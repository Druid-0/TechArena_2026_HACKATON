#include <iostream>
#include <vector>
#include <fstream>
#include <iomanip>

using namespace std;

// In test 5: 125 pipes of size S (S = 1..40)
// In test 9: 1000 pipes of size S (S in 1, 2, 4, 8, 16, 32)
// For each size S:
// Odd pipes: deleted.
// Even pipes: kept.
// If odd pipes are placed at:
// For size S:
// Odd pipes go to temp_pool.
// Even pipes go to perm_pool.

int main() {
    for (int t : {5, 9}) {
        string tf = "tests/" + to_string(t) + ".in";
        ifstream in(tf);
        double w;
        int n;
        in >> w >> n;
        vector<int> ops(n);
        for (int i = 0; i < n; ++i) in >> ops[i];

        int batch_size = (t == 5 ? 125 : 1000);
        int num_even = batch_size / 2;
        int num_odd = batch_size - num_even;

        // Perm pipes start from:
        // Wait, what if Perm pipes start from the END and grow backwards,
        // or Perm pipes start from some offset?
        // In test 5, total perm length = 50840.
        // In test 9, total perm length = 31500.
    }
    return 0;
}
