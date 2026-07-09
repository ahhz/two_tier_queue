The 01-BFS is used for shortest path analysis where the cost of a node or edge is 0 or 1. Typically this is implemented with a deque based priority_queue that places high priority at the front and high priority at the back. Here a solution with two buckets is used, which is a more efficient approach because it avoids the complications of a deque.

`two_tier_queue` is algorithmically equivalent to Dial's algorithm (`k_tier_queue`) specialised to k=2 with a compile-time constant: both maintain two buckets and advance when the current bucket empties. The difference is structural — `two_tier_queue` holds two named `vector<T>` members and swaps them, while `k_tier_queue<T,2>` uses a `vector<vector<T>>` with a circular index. The benchmark confirms they perform at parity; `two_tier_queue` is the more ergonomic choice for the strict 0-1 case, while `k_tier_queue` generalises to any small integer edge weight.

## Benchmark: four-way comparison

This repository includes `benchmark_01_bfs.cpp` to compare four implementations of the priority queue used in 0-1 BFS:

- `std::deque<int>` — the widely-used standard approach, placing weight-0 nodes at the front and weight-1 nodes at the back.
- `two_tier_queue<int>` — the proposed alternative using two plain vectors (current and next tier) swapped when the current tier is exhausted.
- `k_tier_queue<int, 2>` (**compile-time** K) — Dial's algorithm with k=2 known at compile time; the compiler unrolls the two-iteration loop and folds `% 2` to `& 1`. Performs at parity with `two_tier_queue`.
- `k_tier_queue_rt<int>` (**runtime** k=2) — same algorithm but k is a runtime value, preventing loop unrolling and constant folding. Included to quantify the cost of that flexibility.

The benchmark is designed to be fair:

- All four implementations run on identical, pre-generated random 0-1 graphs.
- The execution order cycles through all four queues every round to reduce thermal/turbo bias.
- Warmup rounds are excluded from timing.
- A checksum is validated every round to guarantee all four queues produce identical BFS outputs.

### Build

GCC/Clang:

```bash
g++ -std=c++20 -O3 -DNDEBUG -march=native benchmark_01_bfs.cpp -o bench_01_bfs
```

MSVC (Developer Command Prompt):

```bat
cl /std:c++20 /O2 /DNDEBUG /EHsc benchmark_01_bfs.cpp /Febench_01_bfs.exe
```

### Run

```bash
./bench_01_bfs
```

Useful options:

```text
--cases N           Number of workloads
--base-nodes N      Nodes in first workload
--node-step N       Added nodes per workload
--edges-per-node N  Outgoing edges per node
--sources N         Source nodes per workload
--warmup N          Warmup rounds
--runs N            Timed rounds
--seed N            RNG seed
```

Example:

```bash
./bench_01_bfs --cases 4 --base-nodes 50000 --node-step 25000 --edges-per-node 8 --sources 12 --warmup 2 --runs 10 --seed 42
```

### Sample results

From a Windows/MSVC run with n=100 runs:

```text
.\bench_01_bfs.exe --cases 3 --base-nodes 40000 --node-step 20000 --edges-per-node 8 --sources 8 --warmup 2 --runs 100 --seed 1337
```

**Median timings and speedup:**

```text
0-1 BFS benchmark (same workloads, alternating order, checksummed)
cases=3 base_nodes=40000 node_step=20000 edges_per_node=8 sources=8 warmup=2 runs=100 seed=1337

workload               deque(ms)  two_tier(ms)    k2_ct(ms)   k2_rt(ms)    spd_2t   spd_ct   spd_rt
-------------------------------------------------------------------------------------------------------
case_1_n40000             30.853        17.891       18.822      20.806      1.72x    1.64x    1.48x
case_2_n60000             49.991        30.586       31.826      35.992      1.63x    1.57x    1.39x
case_3_n80000             73.255        44.879       46.412      51.577      1.63x    1.58x    1.42x
-------------------------------------------------------------------------------------------------------
overall(med sum)         154.100        93.356       97.061     108.376      1.65x    1.59x    1.42x
```

**Distribution metrics (standard deviation and coefficient of variation):**

```text
Spread over 100 runs (median ± std dev, CV% = stddev/mean):
                              deque        two_tier           k2_ct           k2_rt
workload              sd(ms)    cv%   sd(ms)    cv%   sd(ms)    cv%   sd(ms)    cv%
------------------------------------------------------------------------------------------
case_1_n40000            2.789   9.0%    1.824  10.0%    1.232   6.5%    2.241  10.6%
case_2_n60000            5.296  10.6%    3.324  10.8%    2.294   7.2%    5.740  15.8%
case_3_n80000            5.074   6.8%    4.278   9.4%    5.782  12.1%    7.563  14.3%
------------------------------------------------------------------------------------------
avg cv%                          8.8%           10.1%            8.6%           13.6%
```

`two_tier_queue` and `k_tier_queue<2>` (compile-time K) perform at parity — both are roughly **1.59–1.72×** faster than `std::deque`. The runtime-k variant (`k_tier_queue_rt`) trails by about 12–17%, confirming the cost of suppressing compile-time optimisations (loop unrolling, constant folding of `% 2`). Over 100 runs, `k_tier_queue<2>` shows the lowest average CV% (8.6%), indicating it is the most stable across workloads.

Million-node run (`n = 1,000,000`):

```text
.\bench_01_bfs.exe --cases 1 --base-nodes 1000000 --node-step 0 --edges-per-node 6 --sources 2 --warmup 1 --runs 2 --seed 42
```

```text
0-1 BFS benchmark (same workloads, alternating order, checksummed)
cases=1 base_nodes=1000000 node_step=0 edges_per_node=6 sources=2 warmup=1 runs=2 seed=42

workload                 deque(ms)  two_tier(ms)    k2_ct(ms)    k2_rt(ms)   spd_2t   spd_ct   spd_rt
-------------------------------------------------------------------------------------------------------
case_1_n1000000           502.333       422.874      446.530      484.607     1.19x    1.12x    1.04x
-------------------------------------------------------------------------------------------------------
overall(avg sum)          502.333       422.874      446.530      484.607     1.19x    1.12x    1.04x
```

At one million nodes the advantage narrows for all three alternatives. `two_tier_queue` and `k_tier_queue<2>` remain close (~1.19x vs ~1.12x), and the runtime-k variant nearly converges with `std::deque` (~1.04x), confirming that the compile-time K is essential to the performance of the Dial's algorithm variant.
