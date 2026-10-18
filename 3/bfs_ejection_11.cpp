#include <iostream>
#include <vector>
#include <fstream>
#include <cstdint>
#include <algorithm>
#include <map>
#include <set>
#include <queue>
#include <chrono>

using namespace std;

const int MAX_X = 22000;
const int TIME_WORDS = 80;

uint64_t occupied[MAX_X][TIME_WORDS];
vector<int> pipes_at_x[MAX_X];

struct Pipe {
    int id, birth, death, len, shift;
    int w_start, w_end;
    uint64_t mask[TIME_WORDS];
};

vector<Pipe> pipes(6494);
int num_pipes;
double test_w;

void remove_pipe(int id) {
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
}

void add_pipe(int id, int s) {
    int l = pipes[id].len;
    for (int x = s; x < s + l; ++x) {
        for (int w = pipes[id].w_start; w <= pipes[id].w_end; ++w) {
            occupied[x][w] |= pipes[id].mask[w];
        }
        pipes_at_x[x].push_back(id);
    }
    pipes[id].shift = s;
}

// Find candidate shifts for pipe `id` within `max_limit`
// Returns pairs of (shift, conflicting_tail_pipe_id), or -1 if 0 conflicts
vector<pair<int, int>> get_ejection_candidates(int id, int max_limit) {
    int l = pipes[id].len;
    int ws = pipes[id].w_start, we = pipes[id].w_end;
    const uint64_t* m = pipes[id].mask;

    vector<pair<int, int>> results;

    int s = 0;
    while (s + l <= max_limit) {
        int prefix_conf_x = -1;
        vector<int> conf_tails;

        for (int x = s; x < s + l; ++x) {
            for (int p_other : pipes_at_x[x]) {
                for (int w = max(ws, pipes[p_other].w_start); w <= min(we, pipes[p_other].w_end); ++w) {
                    if (m[w] & pipes[p_other].mask[w]) {
                        if (p_other <= 4006) {
                            prefix_conf_x = x;
                            break;
                        }
                        conf_tails.push_back(p_other);
                        break;
                    }
                }
                if (prefix_conf_x != -1) break;
            }
            if (prefix_conf_x != -1) break;
        }

        if (prefix_conf_x != -1) {
            s = prefix_conf_x + 1;
            continue;
        }

        sort(conf_tails.begin(), conf_tails.end());
        conf_tails.erase(unique(conf_tails.begin(), conf_tails.end()), conf_tails.end());

        if (conf_tails.empty()) {
            results.push_back({s, -1}); // Free slot!
        } else if (conf_tails.size() == 1) {
            results.push_back({s, conf_tails[0]});
        }
        s++;
    }
    return results;
}

bool solve_pipe_bfs(int target_id, int max_limit, int max_depth) {
    struct Step {
        int pipe_id;
        int shift;
    };
    struct Node {
        int cur_pipe;
        vector<Step> chain;
        vector<int> visited;
    };

    int orig_s = pipes[target_id].shift;
    remove_pipe(target_id);

    queue<Node> q;
    Node root = {target_id, {}, {target_id}};
    q.push(root);

    bool solved = false;
    vector<Step> solution_chain;
    int final_leaf_pipe = -1;
    int final_leaf_shift = -1;

    for (int depth = 1; depth <= max_depth && !solved && !q.empty(); ++depth) {
        int sz = q.size();
        for (int i = 0; i < sz && !solved; ++i) {
            Node cur = q.front();
            q.pop();

            // Temporarily apply chain
            for (const auto& step : cur.chain) {
                add_pipe(step.pipe_id, step.shift);
            }

            auto cands = get_ejection_candidates(cur.cur_pipe, max_limit);

            // Revert chain
            for (auto it = cur.chain.rbegin(); it != cur.chain.rend(); ++it) {
                remove_pipe(it->pipe_id);
            }

            // Check if any candidate is a free slot
            for (const auto& cand : cands) {
                if (cand.second == -1) {
                    // SUCCESS! Leaf has 0 conflicts!
                    solved = true;
                    solution_chain = cur.chain;
                    final_leaf_pipe = cur.cur_pipe;
                    final_leaf_shift = cand.first;
                    break;
                }
            }

            if (solved) break;

            if (depth < max_depth) {
                // Deduplicate by displaced pipe to avoid explosive branching
                map<int, int> best_shift_for_disp;
                for (const auto& cand : cands) {
                    int disp = cand.second;
                    if (disp == -1) continue;
                    if (find(cur.visited.begin(), cur.visited.end(), disp) != cur.visited.end()) continue;
                    if (best_shift_for_disp.find(disp) == best_shift_for_disp.end()) {
                        best_shift_for_disp[disp] = cand.first;
                    }
                }

                for (const auto& [disp, sh] : best_shift_for_disp) {
                    Node next_node;
                    next_node.cur_pipe = disp;
                    next_node.chain = cur.chain;
                    next_node.chain.push_back({cur.cur_pipe, sh});
                    next_node.visited = cur.visited;
                    next_node.visited.push_back(disp);
                    q.push(next_node);
                    if (q.size() > 500) break; // Branching cap
                }
            }
        }
    }

    if (solved) {
        // Apply the solution chain permanently!
        for (const auto& step : solution_chain) {
            add_pipe(step.pipe_id, step.shift);
        }
        add_pipe(final_leaf_pipe, final_leaf_shift);
        cout << "  BFS Chain solved (depth " << solution_chain.size() + 1 << "): ";
        for (const auto& step : solution_chain) {
            cout << step.pipe_id << "->" << step.shift << " ";
        }
        cout << final_leaf_pipe << "->" << final_leaf_shift << endl;
        return true;
    } else {
        add_pipe(target_id, orig_s);
        return false;
    }
}

int main() {
    ifstream in("tests/11.in");
    double w; int n;
    in >> w >> n;
    test_w = w;
    vector<int> ops(n);
    for (int i = 0; i < n; ++i) in >> ops[i];

    vector<uint8_t> perm_bits(16282);
    ifstream f_bits("perm_bits_15.bin", ios::binary);
    f_bits.read((char*)perm_bits.data(), 16282);

    auto is_perm = [&](int pid) -> bool {
        int idx = pid - 1;
        return (perm_bits[idx / 8] >> (idx % 8)) & 1;
    };

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
    num_pipes = pid - 1;

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

    map<int, int> by_start;
    set<pair<int, int>> by_len;
    int trench = 0;

    auto add_free = [&](int s, int l) {
        if (l <= 0) return;
        by_start[s] = l;
        by_len.insert({l, s});
    };
    auto remove_free = [&](int s, int l) {
        by_start.erase(s);
        by_len.erase({l, s});
    };

    pid = 1;
    for (int step = 0; step < 4999; ++step) {
        int op = ops[step];
        if (op > 0) {
            int req = op;
            bool perm = is_perm(pid);
            int best_s = -1, best_l = -1;

            if (perm) {
                auto it = by_len.lower_bound({req, -1});
                if (it != by_len.end() && it->first == req) {
                    best_s = it->second; best_l = it->first;
                } else {
                    auto it2 = by_len.lower_bound({req + 30, -1});
                    if (it2 != by_len.end()) {
                        best_s = it2->second; best_l = it2->first;
                    }
                }
            } else {
                auto it = by_len.lower_bound({req, -1});
                int found_s = -1, found_l = -1;
                int count = 0;
                for (; it != by_len.end() && count < 18; ++it, ++count) {
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
                if (!by_start.empty()) {
                    auto last = prev(by_start.end());
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

            auto it = by_start.lower_bound(s);
            int cur_s = s, cur_l = l;
            if (it != by_start.begin()) {
                auto left = prev(it);
                if (left->first + left->second == cur_s) {
                    cur_s = left->first;
                    cur_l += left->second;
                    remove_free(left->first, left->second);
                }
            }
            auto right = by_start.lower_bound(cur_s + cur_l);
            if (right != by_start.end() && right->first == cur_s + cur_l) {
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

    for (int i = 1; i <= num_pipes; ++i) {
        if (pipes[i].death > 4999) {
            add_pipe(i, pipes[i].shift);
        }
    }

    auto save_plan = [&](int cur_max) {
        int tr = (cur_max + 19) / 20 * 20;
        ofstream out("plan11_tail.bin", ios::binary);
        int tc = num_pipes - online_pipes;
        out.write((char*)&tc, 4);
        for (int i = online_pipes + 1; i <= num_pipes; ++i) {
            out.write((char*)&pipes[i].shift, 4);
        }
        out.close();
        cout << ">>> SAVED plan11_tail.bin! Trench=" << tr 
             << ", max_coord=" << cur_max 
             << ", Score=" << (double)20361 / tr * test_w << endl;
    };

    int cur_max = 0;
    for (int i = 4007; i <= num_pipes; ++i) {
        cur_max = max(cur_max, pipes[i].shift + pipes[i].len);
    }
    cout << "Starting state: max_coord=" << cur_max << ", Trench=" << (cur_max + 19) / 20 * 20 << endl;

    // Iteratively push target limit down in steps of 20
    int best_trench = (cur_max + 19) / 20 * 20;

    for (int target_tr = best_trench - 20; target_tr >= 21460; target_tr -= 20) {
        int limit = target_tr;
        cout << "\n==========================================" << endl;
        cout << "TARGETING TRENCH <= " << target_tr << " (limit coord " << limit << ")" << endl;
        cout << "==========================================" << endl;

        bool round_progress = true;
        while (round_progress) {
            round_progress = false;

            vector<pair<int, int>> over;
            for (int i = 4007; i <= num_pipes; ++i) {
                int end_c = pipes[i].shift + pipes[i].len;
                if (end_c > limit) {
                    over.push_back({end_c, i});
                }
            }
            sort(over.rbegin(), over.rend());

            if (over.empty()) {
                cout << "SUCCESS! All pipes fit <= " << limit << "! Trench=" << target_tr << endl;
                best_trench = target_tr;
                int max_c = 0;
                for (int i = 4007; i <= num_pipes; ++i) max_c = max(max_c, pipes[i].shift + pipes[i].len);
                save_plan(max_c);
                break;
            }

            cout << "Pipes exceeding " << limit << ": " << over.size() << ". Highest: Pipe " 
                 << over[0].second << " (end=" << over[0].first << ", len=" << pipes[over[0].second].len << ")" << endl;

            for (const auto& [end_c, p_id] : over) {
                cout << "  Attempting BFS for Pipe " << p_id << " (end=" << end_c << ")..." << endl;
                bool ok = solve_pipe_bfs(p_id, limit, 3);
                if (ok) {
                    round_progress = true;
                    break; // Recalculate 'over' list
                }
            }

            if (!round_progress) {
                cout << "Could not reduce further below " << target_tr << " with depth 3 BFS." << endl;
                break;
            }
        }

        if (best_trench > target_tr) {
            cout << "Stopping: unable to reach Trench " << target_tr << endl;
            break;
        }
    }

    return 0;
}
