
# Why You Should Stop Using std::deque for 0-1 BFS

If you have ever implemented a 0-1 BFS to solve a shortest-path problem where edge weights are strictly `0` or `1`, you likely reached for `std::deque`. The textbook approach tells you to use a double-ended queue: `push_front()` for `0`-weight edges to process them immediately, and `push_back()` for `1`-weight edges to defer them to the next layer.

It works, it hits the theoretical $O(V + E)$ time complexity, and it passes the tests.

But from a performance perspective, `std::deque` is a drag.

## The Hidden Overhead of std::deque

A standard deque does not store its elements in a single contiguous block of memory. Instead, it allocates memory in a series of non-contiguous fixed-size chunks coordinated via an internal map of pointers.

This chunked architecture introduces two performance liabilities in pathfinding loops:

1. **Pointer Chasing:** Every time you push or pop from a deque, the runtime needs to navigate disconnected memory segments under the hood.
2. **Cache Misses:** Modern CPUs rely on sequential cache-line pre-fetching. Because `std::deque` scatters chunks across the heap, it triggers frequent cache misses, grinding down performance.


## A Cache-Friendly Priority Queue Design

To eliminate this structural fat, we can look to **Dial’s Algorithm** (a bucket-based variant of Dijkstra's). In a 0-1 graph distribution, you only ever need exactly **two buckets** active at any single moment: the current cost tier and the next cost tier.

By capitalizing on this invariant, we can drop `std::deque` entirely and implement a clean queue interface using two contiguous, pre-allocated vectors.

### How it Works

The queue architecture uses two vectors that are the bucket for the `current` and `next` cost level in the queue. When the active `current` vector is fully exhausted, the class performs a constant-time `std::swap(current, next)`. This merely exchanges the internal data pointers of the two underlying vectors. It incurs **zero heap reallocations and zero deep copying**, while automatically incrementing the internal `committed_cost()` tracking.

Within a single cost bucket, the cost of reaching every node is identical. Therefore, extracting elements in a LIFO (Last-In, First-Out) order within that specific bucket does not violate shortest-path correctness. By mapping the front/back pushes to `vector::push_back()`, the class operates purely at the fast tail end of contiguous blocks:

```cpp
vector<T> current, next;
int current_tier = 0; // for convenience we keep track of the tier

inline void push_front(const T& val) {
  current.push_back(val);
}

inline void push_back(const T& val) {
  next.push_back(val);
}

inline const T& top() const {
    return current.empty() ? next.back() : current.back();
}

inline void pop() {
  if(current.empty()) {
    std::swap(current, next); 
    current_tier++;
  }
  current.pop_back();
}
```
## Benchmark

Benchmarking on 01-BFS problems show a computation speed increase factor of about 1.45. 

When running graph traversals on competitive programming judges like LeetCode, swapping a standard `std::deque` 0-1 BFS out for `two_tier_queue` consistently cuts down execution time, frequently hitting the **0ms (Beats 100%)** boundary. For example here: [Find a Safe Walk](https://leetcode.com/problems/find-a-safe-walk-through-a-grid/solutions/8372712/two-bucket-dials-algorithm-0-ms-by-ahhz-d7b6/) 


Check out the becnhmarks and production-ready C++ header under the MIT license on GitHub:

👉 **[GitHub Repository: zero-one-bfs-queue](https://github.com/ahhz/two_tier_queu)**