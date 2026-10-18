#include <iostream>
#include <vector>
#include <cstdint>
#include <cassert>
#include "embedded_plans.h"

using namespace std;

vector<uint16_t> decode_b64(const char* src, int count) {
    static int table[256];
    static bool init = false;
    if (!init) {
        for (int i = 0; i < 256; ++i) table[i] = -1;
        for (int i = 'A'; i <= 'Z'; ++i) table[i] = i - 'A';
        for (int i = 'a'; i <= 'z'; ++i) table[i] = i - 'a' + 26;
        for (int i = '0'; i <= '9'; ++i) table[i] = i - '0' + 52;
        table['+'] = 62;
        table['/'] = 63;
        init = true;
    }

    vector<uint8_t> bytes;
    bytes.reserve(count * 2);

    int val = 0, valb = -8;
    for (const char* c = src; *c && *c != '='; ++c) {
        int d = table[(unsigned char)*c];
        if (d == -1) continue;
        val = (val << 6) | d;
        valb += 6;
        if (valb >= 0) {
            bytes.push_back((uint8_t)((val >> valb) & 0xFF));
            valb -= 8;
        }
    }

    vector<uint16_t> res(count);
    for (int i = 0; i < count; ++i) {
        res[i] = (uint16_t)bytes[2 * i] | ((uint16_t)bytes[2 * i + 1] << 8);
    }
    return res;
}

int main() {
    auto v12 = decode_b64(B64_12, COUNT_12);
    auto v14 = decode_b64(B64_14, COUNT_14);
    auto v13 = decode_b64(B64_13, COUNT_13);
    auto v8  = decode_b64(B64_8,  COUNT_8);

    cout << "Decoded 12: " << v12.size() << " elements. First: " << v12[0] << ", Last: " << v12.back() << endl;
    cout << "Decoded 14: " << v14.size() << " elements. First: " << v14[0] << ", Last: " << v14.back() << endl;
    cout << "Decoded 13: " << v13.size() << " elements. First: " << v13[0] << ", Last: " << v13.back() << endl;
    cout << "Decoded 8:  " << v8.size()  << " elements. First: " << v8[0]  << ", Last: " << v8.back()  << endl;

    // Check shared prefix
    for (int i = 0; i < 210; ++i) {
        assert(v8[i] == v13[i]);
    }
    cout << "Prefix 210 verified identical between 8 and 13!" << endl;
    return 0;
}
