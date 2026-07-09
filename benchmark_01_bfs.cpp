#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <deque>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <type_traits>
#include <vector>

#include "two_tier_queue.hpp"
#include "k_tier_queue.hpp"

// Adapter wrapping k_tier_queue<int, 2> (compile-time K).
struct k_tier_queue_2_adapter {
    k_tier_queue<int, 2> q;
    explicit k_tier_queue_2_adapter(int capacity = 0) : q(capacity) {}
    void push_front(int v) { q.insert(v, 0); }
    void push_back(int v)  { q.insert(v, 1); }
    int  front()     const { return q.top(); }
    void pop_front()       { q.pop(); }
    bool empty()     const { return q.empty(); }
};

// Adapter wrapping k_tier_queue_rt<int> (runtime k=2).
struct k_tier_queue_rt_2_adapter {
    k_tier_queue_rt<int> q;
    explicit k_tier_queue_rt_2_adapter(int capacity = 0) : q(2, capacity) {}
    void push_front(int v) { q.insert(v, 0); }
    void push_back(int v)  { q.insert(v, 1); }
    int  front()     const { return q.top(); }
    void pop_front()       { q.pop(); }
    bool empty()     const { return q.empty(); }
};

struct Edge {
    int to;
    std::uint8_t weight;
};

struct Graph {
    int node_count = 0;
    std::vector<int> offsets;
    std::vector<Edge> edges;
};

struct Workload {
    std::string name;
    Graph graph;
    std::vector<int> sources;
};

struct Config {
    int cases = 3;
    int base_nodes = 40000;
    int node_step = 20000;
    int edges_per_node = 8;
    int sources_per_case = 8;
    int warmup = 2;
    int runs = 100;
    std::uint64_t seed = 1337;
};

struct Scratch {
    std::vector<int> dist;
    std::vector<int> touched;
};

constexpr int kInf = std::numeric_limits<int>::max() / 4;

Graph make_random_graph(int nodes, int edges_per_node, std::uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> node_dist(0, nodes - 1);

    Graph g;
    g.node_count = nodes;
    g.offsets.resize(static_cast<std::size_t>(nodes) + 1, 0);

    const std::size_t total_edges =
        static_cast<std::size_t>(nodes) * static_cast<std::size_t>(edges_per_node);
    g.edges.resize(total_edges);

    std::size_t idx = 0;
    for (int u = 0; u < nodes; ++u) {
        g.offsets[static_cast<std::size_t>(u)] = static_cast<int>(idx);
        for (int k = 0; k < edges_per_node; ++k) {
            const int v = node_dist(rng);
            const std::uint8_t w = static_cast<std::uint8_t>(rng() & 1ULL);
            g.edges[idx++] = Edge{v, w};
        }
    }
    g.offsets[static_cast<std::size_t>(nodes)] = static_cast<int>(idx);
    return g;
}

std::vector<int> make_sources(int count, int nodes, std::uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> node_dist(0, nodes - 1);

    std::vector<int> sources;
    sources.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        sources.push_back(node_dist(rng));
    }
    return sources;
}

template <typename Queue>
Queue make_queue(std::size_t reserve_hint) {
    if constexpr (std::is_constructible_v<Queue, int>) {
        return Queue(static_cast<int>(reserve_hint));
    } else {
        (void)reserve_hint;
        return Queue();
    }
}

template <typename Queue>
std::uint64_t run_single_01_bfs(const Graph &g, int source, Scratch &scratch) {
    auto queue = make_queue<Queue>(static_cast<std::size_t>(g.node_count));

    scratch.touched.clear();
    if (scratch.dist[source] == kInf) {
        scratch.touched.push_back(source);
    }
    scratch.dist[source] = 0;
    queue.push_front(source);

    while (!queue.empty()) {
        const int u = queue.front();
        queue.pop_front();
        const int du = scratch.dist[u];

        const int begin = g.offsets[static_cast<std::size_t>(u)];
        const int end = g.offsets[static_cast<std::size_t>(u) + 1];
        for (int ei = begin; ei < end; ++ei) {
            const Edge &e = g.edges[static_cast<std::size_t>(ei)];
            const int candidate = du + static_cast<int>(e.weight);
            if (candidate < scratch.dist[e.to]) {
                if (scratch.dist[e.to] == kInf) {
                    scratch.touched.push_back(e.to);
                }
                scratch.dist[e.to] = candidate;
                if (e.weight == 0) {
                    queue.push_front(e.to);
                } else {
                    queue.push_back(e.to);
                }
            }
        }
    }

    std::uint64_t checksum = 0x9e3779b97f4a7c15ULL;
    for (int v : scratch.touched) {
        std::uint64_t piece =
            (static_cast<std::uint64_t>(static_cast<std::uint32_t>(v)) << 32) |
            static_cast<std::uint32_t>(scratch.dist[v]);
        piece += 0x9e3779b97f4a7c15ULL;
        piece = (piece ^ (piece >> 30)) * 0xbf58476d1ce4e5b9ULL;
        piece = (piece ^ (piece >> 27)) * 0x94d049bb133111ebULL;
        piece ^= (piece >> 31);
        checksum ^= piece;
    }

    for (int v : scratch.touched) {
        scratch.dist[v] = kInf;
    }

    return checksum;
}

template <typename Queue>
std::uint64_t run_case_once(const Workload &w, Scratch &scratch) {
    std::uint64_t total = 0;
    for (int s : w.sources) {
        total ^= run_single_01_bfs<Queue>(w.graph, s, scratch);
        total = (total << 1) | (total >> 63);
    }
    return total;
}

struct Timing {
    std::uint64_t checksum = 0;
    std::vector<double> samples;

    double median() const {
        std::vector<double> s = samples;
        std::sort(s.begin(), s.end());
        const std::size_t n = s.size();
        return (n % 2 == 0) ? (s[n / 2 - 1] + s[n / 2]) / 2.0 : s[n / 2];
    }
    double mean() const {
        double sum = 0.0;
        for (double v : samples) sum += v;
        return sum / static_cast<double>(samples.size());
    }
    double stddev() const {
        const double m = mean();
        double sq = 0.0;
        for (double v : samples) sq += (v - m) * (v - m);
        return std::sqrt(sq / static_cast<double>(samples.size()));
    }
    double cv() const { return stddev() / mean() * 100.0; }
};

template <typename QueueA, typename QueueB>
std::pair<Timing, Timing> benchmark_case(const Workload &w, const Config &cfg) {
    Scratch scratch_a;
    Scratch scratch_b;
    scratch_a.dist.assign(static_cast<std::size_t>(w.graph.node_count), kInf);
    scratch_b.dist.assign(static_cast<std::size_t>(w.graph.node_count), kInf);
    scratch_a.touched.reserve(static_cast<std::size_t>(w.graph.node_count));
    scratch_b.touched.reserve(static_cast<std::size_t>(w.graph.node_count));

    Timing a;
    Timing b;
    const int total_rounds = cfg.warmup + cfg.runs;

    for (int round = 0; round < total_rounds; ++round) {
        const bool reverse_order = (round & 1) != 0;

        auto run_a = [&]() {
            const auto t0 = std::chrono::steady_clock::now();
            const std::uint64_t chk = run_case_once<QueueA>(w, scratch_a);
            const auto t1 = std::chrono::steady_clock::now();
            if (round >= cfg.warmup) {
                a.samples.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
                a.checksum ^= chk;
            }
            return chk;
        };

        auto run_b = [&]() {
            const auto t0 = std::chrono::steady_clock::now();
            const std::uint64_t chk = run_case_once<QueueB>(w, scratch_b);
            const auto t1 = std::chrono::steady_clock::now();
            if (round >= cfg.warmup) {
                b.samples.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
                b.checksum ^= chk;
            }
            return chk;
        };

        const std::uint64_t chk_a = reverse_order ? run_b() : run_a();
        const std::uint64_t chk_b = reverse_order ? run_a() : run_b();
        if (chk_a != chk_b) {
            std::cerr << "Checksum mismatch in workload '" << w.name << "'.\n";
            std::exit(1);
        }
    }

    return {a, b};
}

template <typename QueueA, typename QueueB, typename QueueC>
std::tuple<Timing, Timing, Timing> benchmark_case_3(const Workload &w, const Config &cfg) {
    Scratch scratch_a, scratch_b, scratch_c;
    scratch_a.dist.assign(static_cast<std::size_t>(w.graph.node_count), kInf);
    scratch_b.dist.assign(static_cast<std::size_t>(w.graph.node_count), kInf);
    scratch_c.dist.assign(static_cast<std::size_t>(w.graph.node_count), kInf);
    scratch_a.touched.reserve(static_cast<std::size_t>(w.graph.node_count));
    scratch_b.touched.reserve(static_cast<std::size_t>(w.graph.node_count));
    scratch_c.touched.reserve(static_cast<std::size_t>(w.graph.node_count));

    Timing a, b, c;
    const int total_rounds = cfg.warmup + cfg.runs;

    for (int round = 0; round < total_rounds; ++round) {
        const int order = round % 3;

        auto run_a = [&]() {
            const auto t0 = std::chrono::steady_clock::now();
            const std::uint64_t chk = run_case_once<QueueA>(w, scratch_a);
            const auto t1 = std::chrono::steady_clock::now();
            if (round >= cfg.warmup) {
                a.samples.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
                a.checksum ^= chk;
            }
            return chk;
        };

        auto run_b = [&]() {
            const auto t0 = std::chrono::steady_clock::now();
            const std::uint64_t chk = run_case_once<QueueB>(w, scratch_b);
            const auto t1 = std::chrono::steady_clock::now();
            if (round >= cfg.warmup) {
                b.samples.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
                b.checksum ^= chk;
            }
            return chk;
        };

        auto run_c = [&]() {
            const auto t0 = std::chrono::steady_clock::now();
            const std::uint64_t chk = run_case_once<QueueC>(w, scratch_c);
            const auto t1 = std::chrono::steady_clock::now();
            if (round >= cfg.warmup) {
                c.samples.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
                c.checksum ^= chk;
            }
            return chk;
        };

        std::uint64_t chk_a, chk_b, chk_c;
        if      (order == 0) { chk_a = run_a(); chk_b = run_b(); chk_c = run_c(); }
        else if (order == 1) { chk_b = run_b(); chk_c = run_c(); chk_a = run_a(); }
        else                 { chk_c = run_c(); chk_a = run_a(); chk_b = run_b(); }

        if (chk_a != chk_b || chk_a != chk_c) {
            std::cerr << "Checksum mismatch in workload '" << w.name << "'.\n";
            std::exit(1);
        }
    }

    return {a, b, c};
}

template <typename QueueA, typename QueueB, typename QueueC, typename QueueD>
std::tuple<Timing, Timing, Timing, Timing> benchmark_case_4(const Workload &w, const Config &cfg) {
    Scratch scratch_a, scratch_b, scratch_c, scratch_d;
    scratch_a.dist.assign(static_cast<std::size_t>(w.graph.node_count), kInf);
    scratch_b.dist.assign(static_cast<std::size_t>(w.graph.node_count), kInf);
    scratch_c.dist.assign(static_cast<std::size_t>(w.graph.node_count), kInf);
    scratch_d.dist.assign(static_cast<std::size_t>(w.graph.node_count), kInf);
    scratch_a.touched.reserve(static_cast<std::size_t>(w.graph.node_count));
    scratch_b.touched.reserve(static_cast<std::size_t>(w.graph.node_count));
    scratch_c.touched.reserve(static_cast<std::size_t>(w.graph.node_count));
    scratch_d.touched.reserve(static_cast<std::size_t>(w.graph.node_count));

    Timing a, b, c, d;
    const int total_rounds = cfg.warmup + cfg.runs;

    for (int round = 0; round < total_rounds; ++round) {
        const int order = round % 4;

        auto run_a = [&]() {
            const auto t0 = std::chrono::steady_clock::now();
            const std::uint64_t chk = run_case_once<QueueA>(w, scratch_a);
            const auto t1 = std::chrono::steady_clock::now();
            if (round >= cfg.warmup) {
                a.samples.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
                a.checksum ^= chk;
            }
            return chk;
        };
        auto run_b = [&]() {
            const auto t0 = std::chrono::steady_clock::now();
            const std::uint64_t chk = run_case_once<QueueB>(w, scratch_b);
            const auto t1 = std::chrono::steady_clock::now();
            if (round >= cfg.warmup) {
                b.samples.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
                b.checksum ^= chk;
            }
            return chk;
        };
        auto run_c = [&]() {
            const auto t0 = std::chrono::steady_clock::now();
            const std::uint64_t chk = run_case_once<QueueC>(w, scratch_c);
            const auto t1 = std::chrono::steady_clock::now();
            if (round >= cfg.warmup) {
                c.samples.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
                c.checksum ^= chk;
            }
            return chk;
        };
        auto run_d = [&]() {
            const auto t0 = std::chrono::steady_clock::now();
            const std::uint64_t chk = run_case_once<QueueD>(w, scratch_d);
            const auto t1 = std::chrono::steady_clock::now();
            if (round >= cfg.warmup) {
                d.samples.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
                d.checksum ^= chk;
            }
            return chk;
        };

        std::uint64_t chk_a, chk_b, chk_c, chk_d;
        if      (order == 0) { chk_a = run_a(); chk_b = run_b(); chk_c = run_c(); chk_d = run_d(); }
        else if (order == 1) { chk_b = run_b(); chk_c = run_c(); chk_d = run_d(); chk_a = run_a(); }
        else if (order == 2) { chk_c = run_c(); chk_d = run_d(); chk_a = run_a(); chk_b = run_b(); }
        else                 { chk_d = run_d(); chk_a = run_a(); chk_b = run_b(); chk_c = run_c(); }

        if (chk_a != chk_b || chk_a != chk_c || chk_a != chk_d) {
            std::cerr << "Checksum mismatch in workload '" << w.name << "'.\n";
            std::exit(1);
        }
    }

    return {a, b, c, d};
}

Config parse_args(int argc, char **argv) {
    Config cfg;
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        auto read_int = [&](int &dst) {
            if (i + 1 >= argc) {
                throw std::runtime_error("Missing value after " + arg);
            }
            dst = std::stoi(argv[++i]);
        };
        auto read_u64 = [&](std::uint64_t &dst) {
            if (i + 1 >= argc) {
                throw std::runtime_error("Missing value after " + arg);
            }
            dst = static_cast<std::uint64_t>(std::stoull(argv[++i]));
        };

        if (arg == "--cases") {
            read_int(cfg.cases);
        } else if (arg == "--base-nodes") {
            read_int(cfg.base_nodes);
        } else if (arg == "--node-step") {
            read_int(cfg.node_step);
        } else if (arg == "--edges-per-node") {
            read_int(cfg.edges_per_node);
        } else if (arg == "--sources") {
            read_int(cfg.sources_per_case);
        } else if (arg == "--warmup") {
            read_int(cfg.warmup);
        } else if (arg == "--runs") {
            read_int(cfg.runs);
        } else if (arg == "--seed") {
            read_u64(cfg.seed);
        } else if (arg == "--help" || arg == "-h") {
            std::cout
                << "Usage: benchmark_01_bfs [options]\n"
                << "  --cases N           Number of workloads (default: 3)\n"
                << "  --base-nodes N      Nodes in first workload (default: 40000)\n"
                << "  --node-step N       Added nodes per workload (default: 20000)\n"
                << "  --edges-per-node N  Outgoing edges per node (default: 8)\n"
                << "  --sources N         Source nodes per workload (default: 8)\n"
                << "  --warmup N          Warmup rounds (default: 2)\n"
                << "  --runs N            Timed rounds (default: 7)\n"
                << "  --seed N            RNG seed (default: 1337)\n";
            std::exit(0);
        } else {
            throw std::runtime_error("Unknown argument: " + arg);
        }
    }
    if (cfg.cases <= 0 || cfg.base_nodes <= 1 || cfg.edges_per_node <= 0 ||
        cfg.sources_per_case <= 0 || cfg.runs <= 0 || cfg.warmup < 0) {
        throw std::runtime_error("Invalid configuration: values must be positive.");
    }
    return cfg;
}

int main(int argc, char **argv) {
    try {
        const Config cfg = parse_args(argc, argv);

        std::vector<Workload> workloads;
        workloads.reserve(static_cast<std::size_t>(cfg.cases));
        for (int i = 0; i < cfg.cases; ++i) {
            const int nodes = cfg.base_nodes + i * cfg.node_step;
            const std::uint64_t graph_seed = cfg.seed + static_cast<std::uint64_t>(i) * 1009ULL;
            const std::uint64_t source_seed = cfg.seed + static_cast<std::uint64_t>(i) * 65537ULL;

            Workload w;
            w.name = "case_" + std::to_string(i + 1) + "_n" + std::to_string(nodes);
            w.graph = make_random_graph(nodes, cfg.edges_per_node, graph_seed);
            w.sources = make_sources(cfg.sources_per_case, nodes, source_seed);
            workloads.push_back(std::move(w));
        }

        std::cout << "0-1 BFS benchmark (same workloads, alternating order, checksummed)\n";
        std::cout << "cases=" << cfg.cases << " base_nodes=" << cfg.base_nodes
                  << " node_step=" << cfg.node_step
                  << " edges_per_node=" << cfg.edges_per_node
                  << " sources=" << cfg.sources_per_case
                  << " warmup=" << cfg.warmup << " runs=" << cfg.runs
                  << " seed=" << cfg.seed << "\n\n";

        // Collect results from all workloads before printing.
        struct WorkloadResult {
            std::string name;
            Timing deque_t, two_tier_t, k2_ct_t, k2_rt_t;
        };
        std::vector<WorkloadResult> results;
        results.reserve(static_cast<std::size_t>(workloads.size()));
        for (const Workload &w : workloads) {
            auto [dt, tt, ct, rt] =
                benchmark_case_4<std::deque<int>, two_tier_queue<int>,
                                 k_tier_queue_2_adapter, k_tier_queue_rt_2_adapter>(w, cfg);
            results.push_back({w.name, std::move(dt), std::move(tt), std::move(ct), std::move(rt)});
        }

        // ---- Table 1: medians and speedup ----
        std::cout << std::left  << std::setw(22) << "workload"
                  << std::right << std::setw(13) << "deque(ms)"
                  << std::setw(15) << "two_tier(ms)"
                  << std::setw(14) << "k2_ct(ms)"
                  << std::setw(14) << "k2_rt(ms)"
                  << std::setw(11) << "spd_2t"
                  << std::setw(11) << "spd_ct"
                  << std::setw(11) << "spd_rt" << "\n";
        std::cout << std::string(111, '-') << "\n";

        double sum_deque = 0.0, sum_two_tier = 0.0, sum_k2_ct = 0.0, sum_k2_rt = 0.0;
        for (const WorkloadResult &r : results) {
            const double md = r.deque_t.median();
            const double mt = r.two_tier_t.median();
            const double mc = r.k2_ct_t.median();
            const double mr = r.k2_rt_t.median();
            sum_deque    += md;
            sum_two_tier += mt;
            sum_k2_ct    += mc;
            sum_k2_rt    += mr;
            std::cout << std::left  << std::setw(22) << r.name
                      << std::right << std::setw(13) << std::fixed << std::setprecision(3)
                      << md << std::setw(15) << mt
                      << std::setw(14) << mc << std::setw(14) << mr
                      << std::setw(10) << std::setprecision(2) << (md / mt) << "x"
                      << std::setw(10) << (md / mc) << "x"
                      << std::setw(10) << (md / mr) << "x\n";
        }
        std::cout << std::string(111, '-') << "\n";
        std::cout << std::left  << std::setw(22) << "overall(med sum)"
                  << std::right << std::setw(13) << std::fixed << std::setprecision(3)
                  << sum_deque << std::setw(15) << sum_two_tier
                  << std::setw(14) << sum_k2_ct << std::setw(14) << sum_k2_rt
                  << std::setw(10) << std::setprecision(2) << (sum_deque / sum_two_tier) << "x"
                  << std::setw(10) << (sum_deque / sum_k2_ct) << "x"
                  << std::setw(10) << (sum_deque / sum_k2_rt) << "x\n";

        // ---- Table 2: spread (std dev + CV%) ----
        std::cout << "\nSpread over " << cfg.runs << " runs (median ± std dev, CV% = stddev/mean):\n";
        std::cout << std::left  << std::setw(22) << ""
                  << std::right
                  << std::setw(16) << "deque"
                  << std::setw(16) << "two_tier"
                  << std::setw(16) << "k2_ct"
                  << std::setw(16) << "k2_rt" << "\n";
        std::cout << std::left  << std::setw(22) << "workload"
                  << std::right
                  << std::setw(9) << "sd(ms)" << std::setw(7) << "cv%"
                  << std::setw(9) << "sd(ms)" << std::setw(7) << "cv%"
                  << std::setw(9) << "sd(ms)" << std::setw(7) << "cv%"
                  << std::setw(9) << "sd(ms)" << std::setw(7) << "cv%" << "\n";
        std::cout << std::string(86, '-') << "\n";

        double sum_cv_d = 0.0, sum_cv_t = 0.0, sum_cv_c = 0.0, sum_cv_r = 0.0;
        for (const WorkloadResult &r : results) {
            const double sd_d = r.deque_t.stddev(),    cv_d = r.deque_t.cv();
            const double sd_t = r.two_tier_t.stddev(), cv_t = r.two_tier_t.cv();
            const double sd_c = r.k2_ct_t.stddev(),    cv_c = r.k2_ct_t.cv();
            const double sd_r = r.k2_rt_t.stddev(),    cv_r = r.k2_rt_t.cv();
            sum_cv_d += cv_d; sum_cv_t += cv_t; sum_cv_c += cv_c; sum_cv_r += cv_r;
            std::cout << std::left  << std::setw(22) << r.name
                      << std::right << std::fixed
                      << std::setw(9) << std::setprecision(3) << sd_d
                      << std::setw(6) << std::setprecision(1) << cv_d << "%"
                      << std::setw(9) << std::setprecision(3) << sd_t
                      << std::setw(6) << std::setprecision(1) << cv_t << "%"
                      << std::setw(9) << std::setprecision(3) << sd_c
                      << std::setw(6) << std::setprecision(1) << cv_c << "%"
                      << std::setw(9) << std::setprecision(3) << sd_r
                      << std::setw(6) << std::setprecision(1) << cv_r << "%\n";
        }
        const double n_cases = static_cast<double>(results.size());
        std::cout << std::string(86, '-') << "\n";
        std::cout << std::left  << std::setw(22) << "avg cv%"
                  << std::right << std::fixed << std::setprecision(1)
                  << std::setw(9) << "" << std::setw(6) << (sum_cv_d / n_cases) << "%"
                  << std::setw(9) << "" << std::setw(6) << (sum_cv_t / n_cases) << "%"
                  << std::setw(9) << "" << std::setw(6) << (sum_cv_c / n_cases) << "%"
                  << std::setw(9) << "" << std::setw(6) << (sum_cv_r / n_cases) << "%\n";

        return 0;
    } catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}

