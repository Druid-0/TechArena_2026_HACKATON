
#include <iostream>
#include <vector>
#include <fstream>
#include <algorithm>
#include <cstdint>

using namespace std;

int main() {
    ifstream in("tests/11.in");
    double w; int n;
    in >> w >> n;
    vector<int> ops(n);
    for (int i = 0; i < n; ++i) in >> ops[i];

    ifstream in_tail("plan11_tail.bin", ios::binary);
    int count;
    in_tail.read((char*)&count, 4);
    vector<int> tail(count);
    in_tail.read((char*)tail.data(), count * 4);
    cout << "plan11_tail has " << count << " shifts." << endl;

    // Verify tail validity and trench
    return 0;
}
