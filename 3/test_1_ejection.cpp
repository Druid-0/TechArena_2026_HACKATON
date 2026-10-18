#include <iostream>
#include <vector>
#include <fstream>
#include <cstdint>
#include <algorithm>
#include <map>
#include <set>

using namespace std;

const int MAX_X = 22000;
const int TIME_WORDS = 80;

uint64_t occupied[MAX_X][TIME_WORDS];
// Also keep track of which pipe occupies (x, w) or spacetime!
// Since multiple pipes occupy x at different times, we can use an array of pipes at each coordinate.
vector<int> pipes_at_x[MAX_X];

struct Pipe {
    int id, birth, death, len, shift;
    int w_start, w_end;
    uint64_t mask[TIME_WORDS];
};

int main() {
    ifstream in("tests/11.in");
    double w; int n;
    in >> w >> n;
    vector<int> ops(n);
    for (int i = 0; i < n; ++i) in >> ops[i];

    vector<uint8_t> perm_bits(16282);
    ifstream f_bits("perm_bits_15.bin", ios::binary);
    f_bits.read((char*)perm_bits.data(), 16282);

    auto is_perm = [&](int pid) -> bool {
        int idx = pid - 1;
        return (perm_bits[idx / 8] >> (idx % 8)) & 1;
    };

    vector<Pipe> pipes(6494);
    for (int i = 1; i <= 6493; ++i) {
        pipes[i].id = i;
        pipes[i].birth = -1;
        pipes[i].death = 1000000000;
        pipes[i].len = 0;
        pipes[i].shift = -1;
        pipes[i].w_start = 0;
        pipes[i].w_end = TIME_WORDS - 1;
        for (int k = 0; k < TIME_WORDS; ++k) pipes[i].mask[k] = 0;
    }

    int pid = 1;
    for (int t = 0; t < n; ++t) {
        if (ops[t] > 0) {
            pipes[pid].birth = t;
            pipes[pid].len = ops[t];
            pid++;
        } else if (ops[t] < 0) {
            pipes[-ops[t]].death = t;
        }
    }
    int num_pipes = pid - 1;

    for (int i = 1; i <= num_pipes; ++i) {
        int t_start = max(4999, pipes[i].birth);
        int t_end = min(n, pipes[i].death);
        if (t_start < t_end) {
            int rel_start = t_start - 4999;
            int rel_end = t_end - 4999;
            pipes[i].w_start = rel_start / 64;
            pipes[i].w_end = (rel_end - 1) / 64;
            for (int t = rel_start; t < rel_end; ++t) {
                pipes[i].mask[t / 64] |= (1ULL << (t % 64));
            }
        }
    }

    map<int, int> m_start;
    set<pair<int, int>> s_len;
    int trench = 0;

    auto add_free = [&](int s, int l) {
        if (l <= 0) return;
        m_start[s] = l;
        s_len.insert({l, s});
    };
    auto remove_free = [&](int s, int l) {
        m_start.erase(s);
        s_len.erase({l, s});
    };

    pid = 1;
    for (int step = 0; step < 4999; ++step) {
        int op = ops[step];
        if (op > 0) {
            int req = op;
            bool perm = is_perm(pid);
            int best_s = -1, best_l = -1;

            if (perm) {
                auto it = s_len.lower_bound({req, -1});
                if (it != s_len.end() && it->first == req) {
                    best_s = it->second; best_l = it->first;
                } else {
                    auto it2 = s_len.lower_bound({req + 30, -1});
                    if (it2 != s_len.end()) {
                        best_s = it2->second; best_l = it2->first;
                    }
                }
            } else {
                auto it = s_len.lower_bound({req, -1});
                int found_s = -1, found_l = -1;
                int count = 0;
                for (; it != s_len.end() && count < 18; ++it, ++count) {
                    if (found_s == -1) { found_s = it->second; found_l = it->first; }
                    if (it->first - req >= 14 || it->first == req) {
                        best_s = it->second; best_l = it->first; break;
                    }
                }
                if (best_s == -1 && found_s != -1) {
                    best_s = found_s; best_l = found_l;
                }
            }

            if (best_s != -1) {
                remove_free(best_s, best_l);
                int rem = best_l - req;
                int place_shift = best_s;
                if (!perm && rem > 0) {
                    place_shift = best_s + rem;
                    add_free(best_s, rem);
                } else {
                    if (rem > 0) add_free(best_s + req, rem);
                }
                pipes[pid].shift = place_shift;
            } else {
                int trailing_s = trench;
                int trailing_len = 0;
                if (!m_start.empty()) {
                    auto last = prev(m_start.end());
                    if (last->first + last->second == trench) {
                        trailing_s = last->first;
                        trailing_len = last->second;
                        remove_free(trailing_s, trailing_len);
                    }
                }
                int needed = req - trailing_len;
                int cnt = (needed + 19) / 20;
                trench += cnt * 20;
                trailing_len += cnt * 20;
                int rem = trailing_len - req;
                if (rem > 0) add_free(trailing_s + req, rem);
                pipes[pid].shift = trailing_s;
            }
            pid++;
        } else {
            int id = -op;
            int s = pipes[id].shift;
            int l = pipes[id].len;

            auto it = m_start.lower_bound(s);
            int cur_s = s, cur_l = l;
            if (it != m_start.begin()) {
                auto left = prev(it);
                if (left->first + left->second == cur_s) {
                    cur_s = left->first;
                    cur_l += left->second;
                    remove_free(left->first, left->second);
                }
            }
            auto right = m_start.lower_bound(cur_s + cur_l);
            if (right != m_start.end() && right->first == cur_s + cur_l) {
                cur_l += right->second;
                remove_free(right->first, right->second);
            }
            add_free(cur_s, cur_l);
        }
    }

    int online_pipes = pid - 1; // 4006

    ifstream f_plan("plan11_tail.bin", ios::binary);
    int tail_count;
    f_plan.read((char*)&tail_count, 4);
    vector<int> tail_shifts(tail_count);
    f_plan.read((char*)tail_shifts.data(), tail_count * 4);

    for (int i = 0; i < tail_count; ++i) {
        pipes[4007 + i].shift = tail_shifts[i];
    }

    auto remove_pipe = [&](int id) {
        int s = pipes[id].shift;
        int l = pipes[id].len;
        for (int x = s; x < s + l; ++x) {
            for (int w = pipes[id].w_start; w <= pipes[id].w_end; ++w) {
                occupied[x][w] &= ~pipes[id].mask[w];
            }
            auto& vec = pipes_at_x[x];
            vec.erase(remove(vec.begin(), vec.end(), id), vec.end());
        }
        pipes[id].shift = -1;
    };

    auto add_pipe = [&](int id, int s) {
        int l = pipes[id].len;
        for (int x = s; x < s + l; ++x) {
            for (int w = pipes[id].w_start; w <= pipes[id].w_end; ++w) {
                occupied[x][w] |= pipes[id].mask[w];
            }
            pipes_at_x[x].push_back(id);
        }
        pipes[id].shift = s;
    };

    for (int i = 1; i <= num_pipes; ++i) {
        if (pipes[i].death > 4999) {
            add_pipe(i, pipes[i].shift);
        }
    }

    auto find_fit = [&](int id, int max_limit) -> int {
        int l = pipes[id].len;
        int ws = pipes[id].w_start, we = pipes[id].w_end;
        const uint64_t* m = pipes[id].mask;

        int s = 0;
        while (s + l <= max_limit) {
            int conf_x = -1;
            for (int x = s; x < s + l; ++x) {
                bool conf = false;
                for (int w = ws; w <= we; ++w) {
                    if (occupied[x][w] & m[w]) { conf = true; break; }
                }
                if (conf) { conf_x = x; break; }
            }
            if (conf_x == -1) return s;
            s = conf_x + 1;
        }
        return -1;
    };

    // Find 1-ejections for each overflowing pipe
    int max_limit = 21460;
    for (int id = 4007; id <= num_pipes; ++id) {
        if (pipes[id].shift + pipes[id].len <= max_limit) continue;

        cout << "Searching 1-ejection for Pipe " << id 
             << " (len=" << pipes[id].len 
             << ", b=" << pipes[id].birth 
             << ", d=" << pipes[id].death 
             << ", cur_s=" << pipes[id].shift << ")..." << endl;

        int orig_s = pipes[id].shift;
        remove_pipe(id);

        int l = pipes[id].len;
        int ws = pipes[id].w_start, we = pipes[id].w_end;
        const uint64_t* m = pipes[id].mask;

        bool solved = false;
        int ejections_tested = 0;

        for (int s = 0; s + l <= max_limit; ++s) {
            // Find all conflicting pipes
            vector<int> conf_pipes;
            bool prefix_conf = false;

            for (int x = s; x < s + l; ++x) {
                for (int p_other : pipes_at_x[x]) {
                    // Check time overlap
                    bool overlap = false;
                    for (int w = max(ws, pipes[p_other].w_start); w <= min(we, pipes[p_other].w_end); ++w) {
                        if (m[w] & pipes[p_other].mask[w]) { overlap = true; break; }
                    }
                    if (overlap) {
                        if (p_other <= 4006) { prefix_conf = true; break; }
                        conf_pipes.push_back(p_other);
                    }
                }
                if (prefix_conf) break;
            }

            if (prefix_conf) continue;
            sort(conf_pipes.begin(), conf_pipes.end());
            conf_pipes.erase(unique(conf_pipes.begin(), conf_pipes.end()), conf_pipes.end());

            if (conf_pipes.size() == 1) {
                int disp = conf_pipes[0];
                ejections_tested++;
                // Try ejecting disp!
                int disp_s = pipes[disp].shift;
                remove_pipe(disp);
                add_pipe(id, s);

                // Can disp find a new place <= max_limit?
                int disp_new_s = find_fit(disp, max_limit);
                if (disp_new_s != -1) {
                    add_pipe(disp, disp_new_s);
                    cout << "  SUCCESS! Pipe " << id << " placed at " << s 
                         << ", displaced Pipe " << disp << " moved from " << disp_s << " to " << disp_new_s << endl;
                    solved = true;
                    break;
                } else {
                    // Undo
                    remove_pipe(id);
                    add_pipe(disp, disp_s);
                }
            }
        }

        if (!solved) {
            add_pipe(id, orig_s);
            cout << "  Failed. Tested " << ejections_tested << " candidate 1-ejections." << endl;
        }
    }

    int remaining = 0;
    int cur_max = 0;
    for (int i = 4007; i <= num_pipes; ++i) {
        int e = pipes[i].shift + pipes[i].len;
        if (e > max_limit) remaining++;
        if (e > cur_max) cur_max = e;
    }
    cout << "Finished! Remaining > 21460: " << remaining << ", current max coord: " << cur_max 
         << ", Trench: " << (cur_max + 19) / 20 * 20 << endl;

    return 0;
}
