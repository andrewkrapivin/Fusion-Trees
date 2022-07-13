#include <boost/container/set.hpp>

#include <random>
#include <chrono>
#include <iostream>
#include <climits>
#include <immintrin.h>
#include <cstdint>
#include <algorithm>
#include <cstring>
#include <set>
#include <cassert>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <string>

#include "../src/SimpleAlloc.h"
#include "../src/fusion_tree.h"
#include "../src/HelperFuncs.h"
#include "../src/FusionBTree.h"
#include "../src/VariableSizeFusionBTree.hpp"
#include "../src/FusionQSort.h"
#include "../src/BenchHelper.hpp"
#include "../src/PrefetchBTree.hpp"

void parallel_insert_items(ParallelFusionBTreeThread pft, __m512i items[], size_t num) {
    for(size_t i = 0; i < num; i++) {
        // cout << i << endl;
        pft.insert(items[i]);
    }
}

void prefetchInsertItems(ParallelBTreeThread pbt, __m512i items[], size_t num) {
    for(size_t i = 0; i < num; i++) {
        // cout << i << endl;
        pbt.insert(items[i]);
    }
}

template<bool chk = false>
void parallel_succ_items(ParallelFusionBTreeThread pft, __m512i items[], size_t num) {
    for(size_t i = 0; i < num; i++) {
        __m512i* test = pft.successor(items[i]);
        if constexpr (chk) assert(i >=num-2 || first_diff_bit_pos(*test, items[i+1]) == -1);
    }
}

template<bool chk = false>
void prefetchSuccessorQueries(ParallelBTreeThread pbt, __m512i items[], size_t num) {
    for(size_t i = 0; i < num; i++) {
        __m512i* test = pbt.successor(items[i]);
        //cout << "succ " << (i+1) << endl;
        if constexpr (chk) assert(i >=num-2 || first_diff_bit_pos(*test, items[i+1]) == -1);
    }
}

template<bool chk = false>
void parallel_pred_items(ParallelFusionBTreeThread pft, __m512i items[], size_t num) {
    for(size_t i = 0; i < num; i++) {
        __m512i* test = pft.predecessor(items[i]);
        // (void)test;
        if constexpr (chk) assert(i == 0 || first_diff_bit_pos(*test, items[i-1]) == -1);
    }
}

int main(int argc, char** argv) {
    unsigned seed = chrono::steady_clock::now().time_since_epoch().count();
    mt19937 generator (seed);
    // rw_lock_init(&root->mtx);

    size_t bigtestsize = 30;
    if(argc >= 2)
        bigtestsize = atoi(argv[1]);
    size_t numThreads = 1;
    if(argc >= 3)
        numThreads = atoi(argv[2]);
    
    ParallelFusionBTree pft{numThreads};
    vector<ParallelFusionBTreeThread> pftThreads;
    for(size_t i{0}; i < numThreads; i++) {
        pftThreads.push_back(ParallelFusionBTreeThread{pft, i});
    }
    
    __m512i* big_randomlist = static_cast<__m512i*>(std::aligned_alloc(64, bigtestsize*64));
    for(size_t i{0}; i < bigtestsize; i++) {
    	big_randomlist[i] = gen_random_vec(generator);
    }

    std::vector<size_t> indices;
    for(size_t i = 0; i <= numThreads; i++) {
        indices.push_back(bigtestsize*i/numThreads);
    }

    BenchHelper bench(numThreads);
    
    ParallelBTree pbt{numThreads};
    vector<ParallelBTreeThread> pbtThreads;
    for(size_t i{0}; i < numThreads; i++) {
        pbtThreads.push_back(ParallelBTreeThread{pbt, i});
    }

    for(size_t i = 0; i < numThreads; i++) {
        bench.addFunctionForThreadTest([=] () -> void {
            parallel_insert_items(pftThreads[i], big_randomlist+indices[i], indices[i+1]-indices[i]);
        });
    }
    bench.timeThreadedFunction("parallel insert");
    
    for(size_t i = 0; i < numThreads; i++) {
        bench.addFunctionForThreadTest([=] () -> void {
            prefetchInsertItems(pbtThreads[i], big_randomlist+indices[i], indices[i+1]-indices[i]);
        });
    }
    bench.timeThreadedFunction("parallel prefetch insert");

    // exit(0);

    bench.timeFunction([&] ()-> void {
        sort(big_randomlist, big_randomlist+bigtestsize, fast_compare__m512i);
    }, "sort on big keys");
    
    for(size_t i = 0; i < numThreads; i++) {
        bench.addFunctionForThreadTest([=] () -> void { prefetchSuccessorQueries<true>(pbtThreads[i], big_randomlist+indices[i], indices[i+1]-indices[i]);});
    }
    bench.timeThreadedFunction("parallel prefetch successor sorted");

    for(size_t i = 0; i < numThreads; i++) {
        bench.addFunctionForThreadTest([=] () -> void { parallel_succ_items<true>(pftThreads[i], big_randomlist+indices[i], indices[i+1]-indices[i]);});
    }
    bench.timeThreadedFunction("parallel successor sorted");

    shuffle(big_randomlist, big_randomlist+bigtestsize, generator);

    for(size_t i = 0; i < numThreads; i++) {
        bench.addFunctionForThreadTest([=] () -> void { prefetchSuccessorQueries(pbtThreads[i], big_randomlist+indices[i], indices[i+1]-indices[i]);});
    }
    bench.timeThreadedFunction("parallel prefetch successor random");

    for(size_t i = 0; i < numThreads; i++) {
        bench.addFunctionForThreadTest([=] () -> void { parallel_succ_items(pftThreads[i], big_randomlist+indices[i], indices[i+1]-indices[i]);});
    }
    bench.timeThreadedFunction("parallel successor random");

    for(size_t i = 0; i < numThreads; i++) {
        bench.addFunctionForThreadTest([=] () -> void { parallel_pred_items(pftThreads[i], big_randomlist+indices[i], indices[i+1]-indices[i]);});
    }
    bench.timeThreadedFunction("parallel predecessor random");
}
