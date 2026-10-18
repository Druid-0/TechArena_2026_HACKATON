// High-performance C++ solver for TechArena 2026
// Features: Single-Office Baseline, Power-Voronoi Partitioning with Worker-Skill Aware DP,
// Adaptive Imbalance Optimization, and Deep Geometry-Aware Local Search.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

// --- Fast JSON Parser ---
std::string read_file(const char* path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open input");
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void skip_ws(const std::string& s, std::size_t& i) {
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
}

struct Json {
    enum class Type { Null, Bool, Number, String, Array, Object };
    Type type = Type::Null;
    double number = 0;
    std::string str;
    std::vector<Json> array;
    std::vector<std::pair<std::string, Json>> object;

    static Json parse(const std::string& s, std::size_t& i) { return parse_value(s, i); }

    static Json parse_value(const std::string& s, std::size_t& i) {
        skip_ws(s, i);
        if (i >= s.size()) throw std::runtime_error("unexpected end");
        char c = s[i];
        if (c == '{') return parse_object(s, i);
        if (c == '[') return parse_array(s, i);
        if (c == '"') return parse_string(s, i);
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parse_number(s, i);
        throw std::runtime_error("bad json");
    }

    static Json parse_object(const std::string& s, std::size_t& i) {
        Json out; out.type = Type::Object; ++i;
        skip_ws(s, i);
        if (i < s.size() && s[i] == '}') { ++i; return out; }
        while (true) {
            Json key = parse_string(s, i);
            skip_ws(s, i);
            if (i >= s.size() || s[i] != ':') throw std::runtime_error("expected :");
            ++i;
            out.object.emplace_back(key.str, parse_value(s, i));
            skip_ws(s, i);
            if (i >= s.size()) throw std::runtime_error("unclosed object");
            if (s[i] == '}') { ++i; return out; }
            if (s[i] != ',') throw std::runtime_error("expected ,");
            ++i;
        }
    }

    static Json parse_array(const std::string& s, std::size_t& i) {
        Json out; out.type = Type::Array; ++i;
        skip_ws(s, i);
        if (i < s.size() && s[i] == ']') { ++i; return out; }
        while (true) {
            out.array.push_back(parse_value(s, i));
            skip_ws(s, i);
            if (i >= s.size()) throw std::runtime_error("unclosed array");
            if (s[i] == ']') { ++i; return out; }
            if (s[i] != ',') throw std::runtime_error("expected ,");
            ++i;
        }
    }

    static Json parse_string(const std::string& s, std::size_t& i) {
        Json out; out.type = Type::String;
        if (s[i] != '"') throw std::runtime_error("expected string");
        ++i;
        while (i < s.size()) {
            char c = s[i++];
            if (c == '"') return out;
            if (c == '\\' && i < s.size()) out.str.push_back(s[i++]);
            else out.str.push_back(c);
        }
        throw std::runtime_error("unterminated string");
    }

    static Json parse_number(const std::string& s, std::size_t& i) {
        Json out; out.type = Type::Number;
        std::size_t start = i;
        if (s[i] == '-') ++i;
        while (i < s.size() && (std::isdigit(static_cast<unsigned char>(s[i])) ||
               s[i] == '.' || s[i] == 'e' || s[i] == 'E' || s[i] == '+' || s[i] == '-')) ++i;
        out.number = std::stod(s.substr(start, i - start));
        return out;
    }

    const Json* find(const std::string& key) const {
        for (const auto& entry : object) if (entry.first == key) return &entry.second;
        return nullptr;
    }
};

// --- Problem Data Structures ---
struct Pt {
    double x, y;
    bool operator<(const Pt& o) const {
        return x < o.x || (x == o.x && y < o.y);
    }
    bool operator==(const Pt& o) const {
        return x == o.x && y == o.y;
    }
};

struct Site {
    int id;
    double x, y, reward;
    std::vector<int> tasks;
    uint64_t skill_mask = 0;
};

struct Office {
    int id;
    double x, y, cost;
    int maxSize;
};

struct Instance {
    std::vector<Site> sites;
    std::vector<Office> offices;
    std::vector<std::vector<int>> workers;
    std::vector<uint64_t> worker_masks;
    std::vector<std::vector<double>> distances;
    int maxDifference = 0;
    double balanceCoefficient = 0.0;
    std::vector<Pt> all_coords; // sites + offices
    uint64_t all_worker_skills = 0;
};

struct OpenedOffice {
    int office_id;
    std::vector<int> sites;
    std::vector<int> workers;
};

using Solution = std::vector<OpenedOffice>;

// --- Geometry Engine (Exact match for Shapely convex_hull.covers) ---
double cross_product(const Pt& O, const Pt& A, const Pt& B) {
    return (A.x - O.x) * (B.y - O.y) - (A.y - O.y) * (B.x - O.x);
}

double dist_sq(const Pt& A, const Pt& B) {
    double dx = A.x - B.x, dy = A.y - B.y;
    return dx * dx + dy * dy;
}

std::vector<Pt> build_convex_hull(std::vector<Pt> pts) {
    std::sort(pts.begin(), pts.end());
    pts.erase(std::unique(pts.begin(), pts.end()), pts.end());
    int n = static_cast<int>(pts.size());
    if (n <= 2) return pts;
    std::vector<Pt> H(2 * n);
    int k = 0;
    for (int i = 0; i < n; ++i) {
        while (k >= 2 && cross_product(H[k - 2], H[k - 1], pts[i]) <= 0.0) k--;
        H[k++] = pts[i];
    }
    for (int i = n - 2, t = k + 1; i >= 0; i--) {
        while (k >= t && cross_product(H[k - 2], H[k - 1], pts[i]) <= 0.0) k--;
        H[k++] = pts[i];
    }
    H.resize(k - 1);
    return H;
}

bool point_on_segment(const Pt& P, const Pt& A, const Pt& B, double eps = 1e-11) {
    if (std::abs(cross_product(A, B, P)) > eps) return false;
    double dot = (P.x - A.x) * (B.x - A.x) + (P.y - A.y) * (B.y - A.y);
    if (dot < -eps) return false;
    double len_sq = dist_sq(A, B);
    if (dot > len_sq + eps) return false;
    return true;
}

bool hull_covers_point(const std::vector<Pt>& hull, const Pt& P, double eps = 1e-11) {
    int n = static_cast<int>(hull.size());
    if (n == 0) return false;
    if (n == 1) return dist_sq(hull[0], P) <= eps * eps;
    if (n == 2) return point_on_segment(P, hull[0], hull[1], eps);
    for (int i = 0; i < n; ++i) {
        const Pt& A = hull[i];
        const Pt& B = hull[(i + 1) % n];
        double cp = cross_product(A, B, P);
        if (cp < -eps) return false;
        if (std::abs(cp) <= eps && point_on_segment(P, A, B, eps)) return true;
    }
    return true;
}

std::vector<uint64_t> get_covered_bitset(const Instance& inst, int office_id, const std::vector<int>& sites) {
    int total_pts = static_cast<int>(inst.all_coords.size());
    int words = (total_pts + 63) / 64;
    std::vector<uint64_t> bits(words, 0ULL);

    std::vector<Pt> sel;
    sel.reserve(sites.size() + 1);
    for (int s : sites) sel.push_back(inst.all_coords[s]);
    sel.push_back(inst.all_coords[inst.sites.size() + office_id]);

    std::vector<Pt> hull = build_convex_hull(std::move(sel));
    for (int i = 0; i < total_pts; ++i) {
        if (hull_covers_point(hull, inst.all_coords[i])) {
            bits[i >> 6] |= (1ULL << (i & 63));
        }
    }
    // Explicitly mark selected points to avoid precision edge-cases
    for (int s : sites) bits[s >> 6] |= (1ULL << (s & 63));
    int oid_idx = static_cast<int>(inst.sites.size()) + office_id;
    bits[oid_idx >> 6] |= (1ULL << (oid_idx & 63));
    return bits;
}

bool bitsets_disjoint(const std::vector<uint64_t>& a, const std::vector<uint64_t>& b) {
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i] & b[i]) return false;
    }
    return true;
}

// --- Objective & Validation ---
double compute_objective(const Instance& inst, const Solution& sol) {
    if (sol.empty()) return 0.0;
    double reward = 0.0, distance = 0.0, cost = 0.0;
    int max_tasks = -1, min_tasks = std::numeric_limits<int>::max();
    for (const auto& off : sol) {
        cost += inst.offices[off.office_id].cost;
        int t_cnt = 0;
        for (int s : off.sites) {
            reward += inst.sites[s].reward;
            distance += inst.distances[off.office_id][s];
            t_cnt += static_cast<int>(inst.sites[s].tasks.size());
        }
        max_tasks = std::max(max_tasks, t_cnt);
        min_tasks = std::min(min_tasks, t_cnt);
    }
    double imbalance = static_cast<double>(max_tasks - min_tasks);
    return reward - distance - cost - inst.balanceCoefficient * imbalance;
}

// --- Robust Worker Assignment (Greedy + Backtracking with MRV) ---
bool assign_workers(const Instance& inst, Solution& sol) {
    int k = static_cast<int>(sol.size());
    int num_w = static_cast<int>(inst.workers.size());
    std::vector<uint64_t> needed_skills(k, 0);
    for (int i = 0; i < k; ++i) {
        sol[i].workers.clear();
        for (int s : sol[i].sites) needed_skills[i] |= inst.sites[s].skill_mask;
    }

    // 1. Fast Greedy
    std::vector<bool> used(num_w, false);
    std::vector<std::vector<int>> greedy_assign(k);
    bool greedy_ok = true;

    for (int i = 0; i < k; ++i) {
        uint64_t needed = needed_skills[i];
        while (needed != 0) {
            int best_w = -1, best_cnt = 0;
            for (int w = 0; w < num_w; ++w) {
                if (used[w]) continue;
                int cnt = __builtin_popcountll(needed & inst.worker_masks[w]);
                if (cnt > best_cnt) {
                    best_cnt = cnt;
                    best_w = w;
                }
            }
            if (best_w == -1) { greedy_ok = false; break; }
            used[best_w] = true;
            greedy_assign[i].push_back(best_w);
            needed &= ~inst.worker_masks[best_w];
        }
        if (!greedy_ok) break;
    }

    if (greedy_ok) {
        for (int i = 0; i < k; ++i) sol[i].workers = std::move(greedy_assign[i]);
        return true;
    }

    // 2. Exact Backtracking
    struct Req { int office; int skill; };
    std::vector<Req> reqs;
    for (int i = 0; i < k; ++i) {
        for (int s = 0; s < 64; ++s) {
            if ((needed_skills[i] >> s) & 1) reqs.push_back({i, s});
        }
    }

    std::vector<bool> b_used(num_w, false);
    std::vector<uint64_t> cur_office_skills(k, 0);
    std::vector<std::vector<int>> cur_assign(k);

    std::function<bool(int)> dfs = [&](int req_idx) -> bool {
        while (req_idx < static_cast<int>(reqs.size()) &&
               ((cur_office_skills[reqs[req_idx].office] >> reqs[req_idx].skill) & 1)) {
            req_idx++;
        }
        if (req_idx == static_cast<int>(reqs.size())) return true;

        int o = reqs[req_idx].office;
        int s = reqs[req_idx].skill;

        for (int w = 0; w < num_w; ++w) {
            if (b_used[w]) continue;
            if ((inst.worker_masks[w] >> s) & 1) {
                b_used[w] = true;
                cur_assign[o].push_back(w);
                uint64_t old_mask = cur_office_skills[o];
                cur_office_skills[o] |= inst.worker_masks[w];

                if (dfs(req_idx + 1)) return true;

                cur_office_skills[o] = old_mask;
                cur_assign[o].pop_back();
                b_used[w] = false;
            }
        }
        return false;
    };

    if (dfs(0)) {
        for (int i = 0; i < k; ++i) sol[i].workers = std::move(cur_assign[i]);
        return true;
    }
    return false;
}

bool is_valid_solution(const Instance& inst, const Solution& sol) {
    int k = static_cast<int>(sol.size());
    if (k == 0) return true;
    std::vector<bool> used_site(inst.sites.size(), false);
    std::vector<bool> used_worker(inst.workers.size(), false);
    std::vector<bool> used_office(inst.offices.size(), false);

    for (const auto& off : sol) {
        if (off.office_id < 0 || off.office_id >= static_cast<int>(inst.offices.size())) return false;
        if (used_office[off.office_id]) return false;
        used_office[off.office_id] = true;
        if (static_cast<int>(off.sites.size()) > inst.offices[off.office_id].maxSize) return false;

        uint64_t w_mask = 0;
        for (int w : off.workers) {
            if (w < 0 || w >= static_cast<int>(inst.workers.size()) || used_worker[w]) return false;
            used_worker[w] = true;
            w_mask |= inst.worker_masks[w];
        }
        for (int s : off.sites) {
            if (s < 0 || s >= static_cast<int>(inst.sites.size()) || used_site[s]) return false;
            used_site[s] = true;
            if ((inst.sites[s].skill_mask & ~w_mask) != 0) return false;
        }
    }
    for (int i = 0; i < k; ++i) {
        for (int j = i + 1; j < k; ++j) {
            if (std::abs(static_cast<int>(sol[i].sites.size()) - static_cast<int>(sol[j].sites.size())) > inst.maxDifference)
                return false;
        }
    }
    std::vector<std::vector<uint64_t>> hulls(k);
    for (int i = 0; i < k; ++i) hulls[i] = get_covered_bitset(inst, sol[i].office_id, sol[i].sites);
    for (int i = 0; i < k; ++i) {
        for (int j = i + 1; j < k; ++j) {
            if (!bitsets_disjoint(hulls[i], hulls[j])) return false;
        }
    }
    return true;
}

// --- Stage 1: Single Office Baseline ---
void solve_single_office(const Instance& inst, Solution& best_sol, double& best_score) {
    for (int o = 0; o < static_cast<int>(inst.offices.size()); ++o) {
        std::vector<std::pair<double, int>> cand;
        for (int s = 0; s < static_cast<int>(inst.sites.size()); ++s) {
            if ((inst.sites[s].skill_mask & ~inst.all_worker_skills) != 0) continue;
            double net = inst.sites[s].reward - inst.distances[o][s];
            if (net > 0.0) cand.emplace_back(net, s);
        }
        std::sort(cand.rbegin(), cand.rend());
        int take = std::min(static_cast<int>(cand.size()), inst.offices[o].maxSize);
        if (take == 0) continue;

        OpenedOffice off;
        off.office_id = o;
        for (int i = 0; i < take; ++i) off.sites.push_back(cand[i].second);
        Solution sol = {off};
        if (assign_workers(inst, sol) && is_valid_solution(inst, sol)) {
            double sc = compute_objective(inst, sol);
            if (sc > best_score) {
                best_score = sc;
                best_sol = std::move(sol);
            }
        }
    }
}

// Reconstruct exact subset of sites for a given office, target site count C, and target task load T
std::vector<int> reconstruct_subset(const Instance& inst, int office_id, const std::vector<int>& cands, int target_c, int target_t) {
    int n = static_cast<int>(cands.size());
    int max_c = target_c;
    int max_t = target_t;
    const double NEG_INF = -1e18;
    int stride = max_t + 1;
    std::vector<double> dp((max_c + 1) * stride, NEG_INF);
    std::vector<int> parent(n * (max_c + 1) * stride, -1);

    dp[0] = 0.0;
    for (int idx = 0; idx < n; ++idx) {
        int s = cands[idx];
        int w_t = static_cast<int>(inst.sites[s].tasks.size());
        double val = inst.sites[s].reward - inst.distances[office_id][s];
        int base_p = idx * (max_c + 1) * stride;
        for (int c = max_c - 1; c >= 0; --c) {
            for (int t = max_t - w_t; t >= 0; --t) {
                double prev = dp[c * stride + t];
                if (prev <= NEG_INF * 0.5) continue;
                int nxt = (c + 1) * stride + (t + w_t);
                if (prev + val > dp[nxt]) {
                    dp[nxt] = prev + val;
                    parent[base_p + nxt] = idx;
                }
            }
        }
    }
    std::vector<int> res;
    int cur_c = target_c, cur_t = target_t;
    for (int idx = n - 1; idx >= 0; --idx) {
        int p = parent[idx * (max_c + 1) * stride + cur_c * stride + cur_t];
        if (p == idx) {
            int s = cands[idx];
            res.push_back(s);
            cur_c -= 1;
            cur_t -= static_cast<int>(inst.sites[s].tasks.size());
        }
    }
    return res;
}

// --- Stage 2: Power-Voronoi Partitioning with Worker-Skill Aware DP ---
void evaluate_partition(const Instance& inst, const std::vector<int>& office_ids,
                        const std::vector<double>& radii, Solution& best_sol, double& best_score) {
    int k = static_cast<int>(office_ids.size());
    std::vector<std::vector<int>> buckets(k);

    // 1. Strict cell containment check for opened offices
    const double margin = 1e-7;
    for (int i = 0; i < k; ++i) {
        Pt pi{inst.offices[office_ids[i]].x, inst.offices[office_ids[i]].y};
        double di = -radii[i];
        for (int j = 0; j < k; ++j) {
            if (i == j) continue;
            Pt pj{inst.offices[office_ids[j]].x, inst.offices[office_ids[j]].y};
            double dj = dist_sq(pi, pj) - radii[j];
            if (dj <= di + margin) return;
        }
    }

    // 2. Assign sites strictly inside Power-Voronoi cells
    std::vector<std::vector<int>> raw_buckets(k);
    for (int s = 0; s < static_cast<int>(inst.sites.size()); ++s) {
        if ((inst.sites[s].skill_mask & ~inst.all_worker_skills) != 0) continue; // Exclude impossible skills
        Pt ps{inst.sites[s].x, inst.sites[s].y};
        int best_i = -1;
        double best_d = 1e18, second_d = 1e18;
        for (int i = 0; i < k; ++i) {
            Pt po{inst.offices[office_ids[i]].x, inst.offices[office_ids[i]].y};
            double d = dist_sq(ps, po) - radii[i];
            if (d < best_d) {
                second_d = best_d;
                best_d = d;
                best_i = i;
            } else if (d < second_d) {
                second_d = d;
            }
        }
        if (best_i != -1 && (second_d - best_d) > margin) {
            double net = inst.sites[s].reward - inst.distances[office_ids[best_i]][s];
            if (net > 0.0) raw_buckets[best_i].push_back(s);
        }
    }

    // 3. Worker-Skill Availability Allocation
    // For each skill with fewer workers than offices, allocate skill to offices with highest demand
    std::vector<int> workers_with_skill(64, 0);
    for (uint64_t m : inst.worker_masks) {
        for (int s = 0; s < 64; ++s) if ((m >> s) & 1) workers_with_skill[s]++;
    }

    std::vector<uint64_t> allowed_skills(k, ~0ULL);
    for (int s = 0; s < 64; ++s) {
        if (workers_with_skill[s] == 0) {
            for (int i = 0; i < k; ++i) allowed_skills[i] &= ~(1ULL << s);
        } else if (workers_with_skill[s] < k) {
            std::vector<std::pair<double, int>> demand(k);
            for (int i = 0; i < k; ++i) {
                double prof = 0.0;
                for (int site_idx : raw_buckets[i]) {
                    if ((inst.sites[site_idx].skill_mask >> s) & 1) {
                        prof += (inst.sites[site_idx].reward - inst.distances[office_ids[i]][site_idx]);
                    }
                }
                demand[i] = {prof, i};
            }
            std::sort(demand.rbegin(), demand.rend());
            for (int r = workers_with_skill[s]; r < k; ++r) {
                allowed_skills[demand[r].second] &= ~(1ULL << s);
            }
        }
    }

    // Filter buckets by allowed skills
    for (int i = 0; i < k; ++i) {
        for (int s : raw_buckets[i]) {
            if ((inst.sites[s].skill_mask & ~allowed_skills[i]) == 0) {
                buckets[i].push_back(s);
            }
        }
        if (buckets[i].empty()) return;
    }

    // 4. Compute 2D DP table (sites, tasks) -> profit
    const double NEG_INF = -1e18;
    std::vector<int> max_T(k, 0);
    std::vector<int> max_C(k, 0);
    std::vector<std::vector<double>> dp_by_T(k);
    std::vector<std::vector<int>> best_C_for_T(k);

    int common_max_T = std::numeric_limits<int>::max();
    for (int i = 0; i < k; ++i) {
        int oid = office_ids[i];
        int cap = std::min(static_cast<int>(buckets[i].size()), inst.offices[oid].maxSize);
        max_C[i] = cap;

        std::sort(buckets[i].begin(), buckets[i].end(), [&](int a, int b) {
            return (inst.sites[a].reward - inst.distances[oid][a]) >
                   (inst.sites[b].reward - inst.distances[oid][b]);
        });

        int t_bound = 0;
        std::vector<int> task_counts;
        for (int s : buckets[i]) task_counts.push_back(static_cast<int>(inst.sites[s].tasks.size()));
        std::sort(task_counts.rbegin(), task_counts.rend());
        for (int j = 0; j < cap; ++j) t_bound += task_counts[j];
        max_T[i] = t_bound;
        common_max_T = std::min(common_max_T, t_bound);

        int stride = t_bound + 1;
        std::vector<double> dp((cap + 1) * stride, NEG_INF);
        dp[0] = 0.0;

        for (int s : buckets[i]) {
            int wt = static_cast<int>(inst.sites[s].tasks.size());
            double val = inst.sites[s].reward - inst.distances[oid][s];
            for (int c = cap - 1; c >= 0; --c) {
                for (int t = t_bound - wt; t >= 0; --t) {
                    if (dp[c * stride + t] > NEG_INF * 0.5) {
                        double cand_v = dp[c * stride + t] + val;
                        if (cand_v > dp[(c + 1) * stride + t + wt]) {
                            dp[(c + 1) * stride + t + wt] = cand_v;
                        }
                    }
                }
            }
        }

        dp_by_T[i].assign(stride, NEG_INF);
        best_C_for_T[i].assign(stride, -1);
        for (int t = 0; t <= t_bound; ++t) {
            for (int c = 1; c <= cap; ++c) {
                if (dp[c * stride + t] > dp_by_T[i][t]) {
                    dp_by_T[i][t] = dp[c * stride + t];
                    best_C_for_T[i][t] = c;
                }
            }
        }
    }

    double open_costs = 0.0;
    for (int oid : office_ids) open_costs += inst.offices[oid].cost;

    // 5. Select Optimal Task Counts (Adaptive to balanceCoefficient)
    std::vector<int> best_chosen_T(k, -1);
    double best_part_score = best_score;

    if (inst.balanceCoefficient >= 1000.0) {
        // High penalty regime (e.g. Instance 0): Enforce zero or 1 task difference
        for (int T = 0; T <= common_max_T; ++T) {
            for (int delta = 0; delta <= 1; ++delta) {
                int T_hi = T + delta;
                if (T_hi > common_max_T) continue;
                double sum_prof = 0.0;
                bool ok = true;
                int min_c = std::numeric_limits<int>::max(), max_c = -1;
                std::vector<int> cur_T(k);
                for (int i = 0; i < k; ++i) {
                    double p = dp_by_T[i][T];
                    int t_pick = T;
                    int c_picked = best_C_for_T[i][T];
                    if (delta == 1 && dp_by_T[i][T_hi] > p) {
                        p = dp_by_T[i][T_hi];
                        t_pick = T_hi;
                        c_picked = best_C_for_T[i][T_hi];
                    }
                    if (p <= NEG_INF * 0.5) { ok = false; break; }
                    sum_prof += p;
                    cur_T[i] = t_pick;
                    min_c = std::min(min_c, c_picked);
                    max_c = std::max(max_c, c_picked);
                }
                if (!ok || (max_c - min_c) > inst.maxDifference) continue;
                double total_sc = sum_prof - open_costs - inst.balanceCoefficient * delta;
                if (total_sc > best_part_score) {
                    best_part_score = total_sc;
                    best_chosen_T = cur_T;
                }
            }
        }
    } else if (k == 2) {
        // Low penalty regime for 2 offices: Full exact joint search
        for (int t0 = 0; t0 <= max_T[0]; ++t0) {
            double p0 = dp_by_T[0][t0];
            if (p0 <= NEG_INF * 0.5) continue;
            int c0 = best_C_for_T[0][t0];
            for (int t1 = 0; t1 <= max_T[1]; ++t1) {
                double p1 = dp_by_T[1][t1];
                if (p1 <= NEG_INF * 0.5) continue;
                int c1 = best_C_for_T[1][t1];
                if (std::abs(c0 - c1) > inst.maxDifference) continue;
                double sc = p0 + p1 - open_costs - inst.balanceCoefficient * std::abs(t0 - t1);
                if (sc > best_part_score) {
                    best_part_score = sc;
                    best_chosen_T = {t0, t1};
                }
            }
        }
    } else {
        // Low penalty regime for k >= 3 offices: Sliding window over task spans
        std::vector<int> window_sizes = {0, 1, 2, 5, 10, 20, 40, 80};
        int global_max_T = 0;
        for (int i = 0; i < k; ++i) global_max_T = std::max(global_max_T, max_T[i]);

        for (int w : window_sizes) {
            for (int t_start = 0; t_start <= global_max_T; t_start += std::max(1, w / 2)) {
                int t_end = t_start + w;
                double sum_prof = 0.0;
                bool ok = true;
                int min_c = std::numeric_limits<int>::max(), max_c = -1;
                int actual_min_t = std::numeric_limits<int>::max(), actual_max_t = -1;
                std::vector<int> cur_T(k);

                for (int i = 0; i < k; ++i) {
                    double best_p = NEG_INF;
                    int best_t = -1;
                    int best_c = -1;
                    int lim = std::min(t_end, max_T[i]);
                    for (int t = t_start; t <= lim; ++t) {
                        if (dp_by_T[i][t] > best_p) {
                            best_p = dp_by_T[i][t];
                            best_t = t;
                            best_c = best_C_for_T[i][t];
                        }
                    }
                    if (best_p <= NEG_INF * 0.5) { ok = false; break; }
                    sum_prof += best_p;
                    cur_T[i] = best_t;
                    min_c = std::min(min_c, best_c);
                    max_c = std::max(max_c, best_c);
                    actual_min_t = std::min(actual_min_t, best_t);
                    actual_max_t = std::max(actual_max_t, best_t);
                }
                if (!ok || (max_c - min_c) > inst.maxDifference) continue;
                double total_sc = sum_prof - open_costs - inst.balanceCoefficient * (actual_max_t - actual_min_t);
                if (total_sc > best_part_score) {
                    best_part_score = total_sc;
                    best_chosen_T = cur_T;
                }
            }
        }
    }

    if (best_chosen_T[0] != -1) {
        Solution cand_sol(k);
        for (int i = 0; i < k; ++i) {
            int oid = office_ids[i];
            int t_use = best_chosen_T[i];
            int c_use = best_C_for_T[i][t_use];
            cand_sol[i].office_id = oid;
            cand_sol[i].sites = reconstruct_subset(inst, oid, buckets[i], c_use, t_use);
        }
        if (assign_workers(inst, cand_sol) && is_valid_solution(inst, cand_sol)) {
            double real_sc = compute_objective(inst, cand_sol);
            if (real_sc > best_score) {
                best_score = real_sc;
                best_sol = std::move(cand_sol);
            }
        }
    }
}

// --- Stage 3: Deep Geometry-Aware Local Search (Hill-Climbing / Simulated Annealing) ---
void local_search_refine(const Instance& inst, Solution& best_sol, double& best_score,
                         std::chrono::steady_clock::time_point deadline) {
    if (best_sol.empty()) return;
    int k = static_cast<int>(best_sol.size());

    // Single office optimization
    if (k == 1) {
        int oid = best_sol[0].office_id;
        std::vector<bool> in_sol(inst.sites.size(), false);
        for (int s : best_sol[0].sites) in_sol[s] = true;

        bool added = true;
        while (added && std::chrono::steady_clock::now() < deadline) {
            added = false;
            if (static_cast<int>(best_sol[0].sites.size()) >= inst.offices[oid].maxSize) break;
            int best_s = -1;
            double best_gain = 0.0;
            for (int s = 0; s < static_cast<int>(inst.sites.size()); ++s) {
                if (in_sol[s]) continue;
                if ((inst.sites[s].skill_mask & ~inst.all_worker_skills) != 0) continue;
                double gain = inst.sites[s].reward - inst.distances[oid][s];
                if (gain > best_gain) {
                    // Check worker assignment with this site added
                    Solution cand = best_sol;
                    cand[0].sites.push_back(s);
                    if (assign_workers(inst, cand)) {
                        best_gain = gain;
                        best_s = s;
                    }
                }
            }
            if (best_s != -1) {
                best_sol[0].sites.push_back(best_s);
                in_sol[best_s] = true;
                assign_workers(inst, best_sol);
                best_score = compute_objective(inst, best_sol);
                added = true;
            }
        }
        return;
    }

    // Multi-office deep local search
    Solution cur = best_sol;
    double cur_score = best_score;
    std::vector<bool> in_sol(inst.sites.size(), false);
    std::vector<int> task_loads(k, 0);
    std::vector<std::vector<uint64_t>> hulls(k);

    for (int i = 0; i < k; ++i) {
        for (int s : cur[i].sites) {
            in_sol[s] = true;
            task_loads[i] += static_cast<int>(inst.sites[s].tasks.size());
        }
        hulls[i] = get_covered_bitset(inst, cur[i].office_id, cur[i].sites);
    }

    std::mt19937 rng(1337);
    int no_improve = 0;

    while (std::chrono::steady_clock::now() < deadline && no_improve < 20000) {
        no_improve++;
        int move_type = rng() % 4;

        if (move_type == 0) {
            // Operator 1: Add an unassigned site
            int s = rng() % inst.sites.size();
            if (in_sol[s] || (inst.sites[s].skill_mask & ~inst.all_worker_skills) != 0) continue;
            int i = rng() % k;
            int oid = cur[i].office_id;

            if (static_cast<int>(cur[i].sites.size()) + 1 > inst.offices[oid].maxSize) continue;
            bool size_ok = true;
            for (int j = 0; j < k; ++j) {
                if (i != j && std::abs(static_cast<int>(cur[i].sites.size()) + 1 - static_cast<int>(cur[j].sites.size())) > inst.maxDifference) {
                    size_ok = false; break;
                }
            }
            if (!size_ok) continue;

            auto tmp_sites = cur[i].sites;
            tmp_sites.push_back(s);
            auto nh = get_covered_bitset(inst, oid, tmp_sites);
            bool hull_ok = true;
            for (int j = 0; j < k; ++j) {
                if (i != j && !bitsets_disjoint(nh, hulls[j])) { hull_ok = false; break; }
            }
            if (!hull_ok) continue;

            Solution cand = cur;
            cand[i].sites = std::move(tmp_sites);
            if (!assign_workers(inst, cand)) continue;

            double new_sc = compute_objective(inst, cand);
            if (new_sc > cur_score) {
                cur = std::move(cand);
                cur_score = new_sc;
                hulls[i] = std::move(nh);
                in_sol[s] = true;
                no_improve = 0;
                if (cur_score > best_score) {
                    best_score = cur_score;
                    best_sol = cur;
                }
            }
        } else if (move_type == 1) {
            // Operator 2: Swap a site with an unassigned site
            int i = rng() % k;
            if (cur[i].sites.empty()) continue;
            int idx = rng() % cur[i].sites.size();
            int old_s = cur[i].sites[idx];
            int new_s = rng() % inst.sites.size();
            if (in_sol[new_s] || (inst.sites[new_s].skill_mask & ~inst.all_worker_skills) != 0) continue;

            int oid = cur[i].office_id;
            auto tmp_sites = cur[i].sites;
            tmp_sites[idx] = new_s;
            auto nh = get_covered_bitset(inst, oid, tmp_sites);
            bool hull_ok = true;
            for (int j = 0; j < k; ++j) {
                if (i != j && !bitsets_disjoint(nh, hulls[j])) { hull_ok = false; break; }
            }
            if (!hull_ok) continue;

            Solution cand = cur;
            cand[i].sites = std::move(tmp_sites);
            if (!assign_workers(inst, cand)) continue;

            double new_sc = compute_objective(inst, cand);
            if (new_sc > cur_score) {
                cur = std::move(cand);
                cur_score = new_sc;
                hulls[i] = std::move(nh);
                in_sol[old_s] = false;
                in_sol[new_s] = true;
                no_improve = 0;
                if (cur_score > best_score) {
                    best_score = cur_score;
                    best_sol = cur;
                }
            }
        } else if (move_type == 2) {
            // Operator 3: Move a site between two opened offices
            int from = rng() % k;
            int to = rng() % k;
            if (from == to || cur[from].sites.empty()) continue;
            int idx = rng() % cur[from].sites.size();
            int s = cur[from].sites[idx];

            int to_oid = cur[to].office_id;
            if (static_cast<int>(cur[to].sites.size()) + 1 > inst.offices[to_oid].maxSize) continue;

            // Check size difference
            int new_from_sz = static_cast<int>(cur[from].sites.size()) - 1;
            int new_to_sz = static_cast<int>(cur[to].sites.size()) + 1;
            bool size_ok = true;
            for (int j = 0; j < k; ++j) {
                int sz_j = (j == from ? new_from_sz : (j == to ? new_to_sz : static_cast<int>(cur[j].sites.size())));
                for (int l = j + 1; l < k; ++l) {
                    int sz_l = (l == from ? new_from_sz : (l == to ? new_to_sz : static_cast<int>(cur[l].sites.size())));
                    if (std::abs(sz_j - sz_l) > inst.maxDifference) { size_ok = false; break; }
                }
                if (!size_ok) break;
            }
            if (!size_ok) continue;

            auto to_sites = cur[to].sites;
            to_sites.push_back(s);
            auto nh_to = get_covered_bitset(inst, to_oid, to_sites);

            auto from_sites = cur[from].sites;
            from_sites.erase(from_sites.begin() + idx);
            auto nh_from = get_covered_bitset(inst, cur[from].office_id, from_sites);

            bool hull_ok = bitsets_disjoint(nh_from, nh_to);
            if (hull_ok) {
                for (int j = 0; j < k; ++j) {
                    if (j != from && j != to) {
                        if (!bitsets_disjoint(nh_to, hulls[j]) || !bitsets_disjoint(nh_from, hulls[j])) {
                            hull_ok = false; break;
                        }
                    }
                }
            }
            if (!hull_ok) continue;

            Solution cand = cur;
            cand[from].sites = std::move(from_sites);
            cand[to].sites = std::move(to_sites);
            if (!assign_workers(inst, cand)) continue;

            double new_sc = compute_objective(inst, cand);
            if (new_sc > cur_score) {
                cur = std::move(cand);
                cur_score = new_sc;
                hulls[from] = std::move(nh_from);
                hulls[to] = std::move(nh_to);
                no_improve = 0;
                if (cur_score > best_score) {
                    best_score = cur_score;
                    best_sol = cur;
                }
            }
        } else {
            // Operator 4: Remove a site (if it improves task imbalance or net profit)
            int i = rng() % k;
            if (cur[i].sites.size() <= 1) continue;
            int idx = rng() % cur[i].sites.size();
            int s = cur[i].sites[idx];

            int new_sz = static_cast<int>(cur[i].sites.size()) - 1;
            bool size_ok = true;
            for (int j = 0; j < k; ++j) {
                if (i != j && std::abs(new_sz - static_cast<int>(cur[j].sites.size())) > inst.maxDifference) {
                    size_ok = false; break;
                }
            }
            if (!size_ok) continue;

            auto tmp_sites = cur[i].sites;
            tmp_sites.erase(tmp_sites.begin() + idx);
            auto nh = get_covered_bitset(inst, cur[i].office_id, tmp_sites);

            Solution cand = cur;
            cand[i].sites = std::move(tmp_sites);
            if (!assign_workers(inst, cand)) continue;

            double new_sc = compute_objective(inst, cand);
            if (new_sc > cur_score) {
                cur = std::move(cand);
                cur_score = new_sc;
                hulls[i] = std::move(nh);
                in_sol[s] = false;
                no_improve = 0;
                if (cur_score > best_score) {
                    best_score = cur_score;
                    best_sol = cur;
                }
            }
        }
    }
}

Instance parse_instance(const Json& root) {
    Instance inst;
    const Json* sites_j = root.find("sites");
    const Json* offices_j = root.find("offices");
    const Json* workers_j = root.find("workers");
    const Json* dist_j = root.find("distances");
    const Json* max_diff_j = root.find("maxDifference");
    const Json* bal_j = root.find("balanceCoefficient");

    if (max_diff_j) inst.maxDifference = static_cast<int>(max_diff_j->number);
    if (bal_j) inst.balanceCoefficient = bal_j->number;

    if (sites_j) {
        int id = 0;
        for (const auto& sj : sites_j->array) {
            Site s;
            s.id = id++;
            s.x = sj.find("x")->number;
            s.y = sj.find("y")->number;
            s.reward = sj.find("reward")->number;
            for (const auto& tj : sj.find("tasks")->array) {
                int t = static_cast<int>(tj.number);
                s.tasks.push_back(t);
                s.skill_mask |= (1ULL << t);
            }
            inst.sites.push_back(std::move(s));
        }
    }
    if (offices_j) {
        int id = 0;
        for (const auto& oj : offices_j->array) {
            Office o;
            o.id = id++;
            o.x = oj.find("x")->number;
            o.y = oj.find("y")->number;
            o.cost = oj.find("cost")->number;
            o.maxSize = static_cast<int>(oj.find("maxSize")->number);
            inst.offices.push_back(o);
        }
    }
    if (workers_j) {
        for (const auto& wj : workers_j->array) {
            std::vector<int> w;
            uint64_t mask = 0;
            for (const auto& sk : wj.array) {
                int skill = static_cast<int>(sk.number);
                w.push_back(skill);
                mask |= (1ULL << skill);
            }
            inst.workers.push_back(std::move(w));
            inst.worker_masks.push_back(mask);
            inst.all_worker_skills |= mask;
        }
    }
    if (dist_j) {
        for (const auto& row : dist_j->array) {
            std::vector<double> r;
            r.reserve(row.array.size());
            for (const auto& val : row.array) r.push_back(val.number);
            inst.distances.push_back(std::move(r));
        }
    }
    for (const auto& s : inst.sites) inst.all_coords.push_back({s.x, s.y});
    for (const auto& o : inst.offices) inst.all_coords.push_back({o.x, o.y});
    return inst;
}

std::string serialize_solution(const Solution& sol) {
    std::ostringstream out;
    out << "[\n";
    for (std::size_t i = 0; i < sol.size(); ++i) {
        const auto& off = sol[i];
        out << "\t{\n\t\t\"officeId\": " << off.office_id << ",\n\t\t\"sites\": [";
        for (std::size_t j = 0; j < off.sites.size(); ++j) {
            if (j) out << ", ";
            out << off.sites[j];
        }
        out << "],\n\t\t\"workers\": [";
        for (std::size_t j = 0; j < off.workers.size(); ++j) {
            if (j) out << ", ";
            out << off.workers[j];
        }
        out << "]\n\t}" << (i + 1 < sol.size() ? "," : "") << "\n";
    }
    out << "]\n";
    return out.str();
}

} // namespace

int main(int argc, char** argv) {
    auto start_time = std::chrono::steady_clock::now();
    auto deadline = start_time + std::chrono::seconds(275); // Safe margin under 300s limit

    try {
        const char* input_path = argc > 1 ? argv[1] : "instance.json";
        const char* output_path = argc > 2 ? argv[2] : "solution.json";
        std::string text = read_file(input_path);
        if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
            static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF) {
            text.erase(0, 3);
        }
        std::size_t pos = 0;
        Instance inst = parse_instance(Json::parse(text, pos));

        Solution best_sol;
        double best_score = 0.0;

        // 1. Single office baseline
        solve_single_office(inst, best_sol, best_score);

        // 2. Rank offices by individual potential
        int n_off = static_cast<int>(inst.offices.size());
        std::vector<std::pair<double, int>> off_rank;
        for (int o = 0; o < n_off; ++o) {
            std::vector<double> nets;
            for (int s = 0; s < static_cast<int>(inst.sites.size()); ++s) {
                if ((inst.sites[s].skill_mask & ~inst.all_worker_skills) != 0) continue;
                double v = inst.sites[s].reward - inst.distances[o][s];
                if (v > 0.0) nets.push_back(v);
            }
            std::sort(nets.rbegin(), nets.rend());
            double sum = -inst.offices[o].cost;
            for (int i = 0; i < std::min(static_cast<int>(nets.size()), inst.offices[o].maxSize); ++i)
                sum += nets[i];
            off_rank.emplace_back(sum, o);
        }
        std::sort(off_rank.rbegin(), off_rank.rend());

        // 3. Systematic Pairs Exploration with multiple Power-Diagram shifts
        std::vector<double> shifts = {0.0, -0.04, 0.04, -0.10, 0.10, -0.18, 0.18, -0.30, 0.30};
        for (int i = 0; i < n_off; ++i) {
            for (int j = i + 1; j < n_off; ++j) {
                if (std::chrono::steady_clock::now() >= deadline) break;
                int o1 = off_rank[i].second;
                int o2 = off_rank[j].second;
                for (double r2 : shifts) {
                    evaluate_partition(inst, {o1, o2}, {0.0, r2}, best_sol, best_score);
                }
            }
        }

        // 4. Triples Exploration with randomized Power-Diagram radii
        int top_m = std::min(n_off, 11);
        std::mt19937 rng(42);
        std::uniform_real_distribution<double> r_dist(-0.25, 0.25);

        for (int i = 0; i < top_m; ++i) {
            for (int j = i + 1; j < top_m; ++j) {
                for (int l = j + 1; l < top_m; ++l) {
                    if (std::chrono::steady_clock::now() >= deadline) break;
                    int o1 = off_rank[i].second, o2 = off_rank[j].second, o3 = off_rank[l].second;
                    evaluate_partition(inst, {o1, o2, o3}, {0.0, 0.0, 0.0}, best_sol, best_score);
                    for (int t = 0; t < 8; ++t) {
                        evaluate_partition(inst, {o1, o2, o3}, {0.0, r_dist(rng), r_dist(rng)}, best_sol, best_score);
                    }
                }
            }
        }

        // 5. Quadruples Exploration
        int top_q = std::min(n_off, 9);
        for (int i = 0; i < top_q; ++i) {
            for (int j = i + 1; j < top_q; ++j) {
                for (int l = j + 1; l < top_q; ++l) {
                    for (int m = l + 1; m < top_q; ++m) {
                        if (std::chrono::steady_clock::now() >= deadline) break;
                        int o1 = off_rank[i].second, o2 = off_rank[j].second;
                        int o3 = off_rank[l].second, o4 = off_rank[m].second;
                        evaluate_partition(inst, {o1, o2, o3, o4}, {0.0, 0.0, 0.0, 0.0}, best_sol, best_score);
                        for (int t = 0; t < 6; ++t) {
                            evaluate_partition(inst, {o1, o2, o3, o4},
                                               {0.0, r_dist(rng), r_dist(rng), r_dist(rng)}, best_sol, best_score);
                        }
                    }
                }
            }
        }

        // 6. 5-Tuples Exploration
        int top_5 = std::min(n_off, 7);
        for (int i = 0; i < top_5; ++i) {
            for (int j = i + 1; j < top_5; ++j) {
                for (int l = j + 1; l < top_5; ++l) {
                    for (int m = l + 1; m < top_5; ++m) {
                        for (int p = m + 1; p < top_5; ++p) {
                            if (std::chrono::steady_clock::now() >= deadline) break;
                            int o1 = off_rank[i].second, o2 = off_rank[j].second;
                            int o3 = off_rank[l].second, o4 = off_rank[m].second, o5 = off_rank[p].second;
                            evaluate_partition(inst, {o1, o2, o3, o4, o5}, {0.0, 0.0, 0.0, 0.0, 0.0}, best_sol, best_score);
                            for (int t = 0; t < 4; ++t) {
                                evaluate_partition(inst, {o1, o2, o3, o4, o5},
                                                   {0.0, r_dist(rng), r_dist(rng), r_dist(rng), r_dist(rng)}, best_sol, best_score);
                            }
                        }
                    }
                }
            }
        }

        // 7. Deep Local Search Refinement on Best Solution Found
        local_search_refine(inst, best_sol, best_score, deadline);

        std::ofstream output(output_path, std::ios::binary);
        if (!output) return 1;
        output << serialize_solution(best_sol);
        return output.good() ? 0 : 1;
    } catch (...) {
        return 1;
    }
}