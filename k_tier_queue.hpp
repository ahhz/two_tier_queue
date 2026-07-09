/*
 MIT License
 Copyright (c) Alex Hagen-Zanker, 2026 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 */

#ifndef K_TIER_QUEUE_HPP
#define K_TIER_QUEUE_HPP

#include <vector>
#include <utility>
#include <stdexcept>


/**
 * @class k_tier_queue
 * @brief A priority queue that holds at most K tiers of priority
 * simultaneously. On pop_front(), if the current tier is exhausted, the 
 * next tier becomes the current tier.  This is useful for implementing Dials's 
 * algorithm for shortest paths where edge weights are small integers.
 */

template<class T, int K>
class k_tier_queue {
private:
    std::vector<std::vector<T>> tiers; // Vector of buckets for each tier
    int current_tier;         // The current global path cost level
   
public:
    k_tier_queue(int capacity = 0) : current_tier(0) {
        tiers.resize(K);
        for(auto& tier : tiers) {
            if(capacity > 0) {
                tier.reserve(capacity);
            }
        }
    }
   
    inline void insert(const T& val, int weight) {
        tiers[(current_tier + weight) % K].push_back(val);
    }

    inline bool empty() const {
        for(const auto& tier : tiers) {
           if(!tier.empty()) return false;
        }
        return true;
    }

    /**
    * @brief Returns the next node with highest priority without removing it.
    * @throws std::out_of_range if queue is empty.
    */
    inline const T& top() const {
       int tt = current_tier;
       for(int i = 0; i < K; i++, tt++) {
           if(!tiers[tt % K].empty()) return tiers[tt % K].back();
       }
       throw std::out_of_range("Accessing top of an empty queue.");
    }
    inline int top_tier() const {
        int tt = current_tier;
        for(int i = 0; i < K; i++, tt++) {
           if(!tiers[tt % K].empty()) return tt;
        }
        throw std::out_of_range("Accessing top of an empty queue.");
    }

    /**
    * @brief Removes the node with the highest priority.
    * @throws std::out_of_range if queue is empty.
    */
    inline void pop() {
       for(int i = 0; i < K; i++) {
           auto& tier = tiers[current_tier % K];
           if(!tier.empty()) {
               tier.pop_back();
               if(tier.empty()) {
                   current_tier++;
               }
               return;
           } else {
               current_tier++;
           }    
       }
       throw std::out_of_range("Popping an empty queue.");
    }
    
    inline int tier() const {
        return current_tier;
    }
  
};

/**
 * @class k_tier_queue_rt
 * @brief Same as k_tier_queue but with k supplied at runtime. Useful when the
 * number of tiers is not known at compile time, at the cost of preventing loop
 * unrolling and compile-time folding of the modulo operation.
 */
template<class T>
class k_tier_queue_rt {
private:
    std::vector<std::vector<T>> tiers;
    int current_tier;
    int k;

public:
    k_tier_queue_rt(int k, int capacity = 0) : k(k), current_tier(0) {
        tiers.resize(k);
        for(auto& tier : tiers) {
            if(capacity > 0) {
                tier.reserve(capacity);
            }
        }
    }

    inline void insert(const T& val, int weight) {
        tiers[(current_tier + weight) % k].push_back(val);
    }

    inline bool empty() const {
        for(const auto& tier : tiers) {
           if(!tier.empty()) return false;
        }
        return true;
    }

    inline const T& top() const {
       int tt = current_tier;
       for(int i = 0; i < k; i++, tt++) {
           if(!tiers[tt % k].empty()) return tiers[tt % k].back();
       }
       throw std::out_of_range("Accessing top of an empty queue.");
    }

    inline int top_tier() const {
        int tt = current_tier;
        for(int i = 0; i < k; i++, tt++) {
           if(!tiers[tt % k].empty()) return tt;
        }
        throw std::out_of_range("Accessing top of an empty queue.");
    }

    inline void pop() {
       for(int i = 0; i < k; i++) {
           auto& tier = tiers[current_tier % k];
           if(!tier.empty()) {
               tier.pop_back();
               if(tier.empty()) {
                   current_tier++;
               }
               return;
           } else {
               current_tier++;
           }
       }
       throw std::out_of_range("Popping an empty queue.");
    }

    inline int tier() const {
        return current_tier;
    }
};

#endif // K_TIER_QUEUE_HPP
