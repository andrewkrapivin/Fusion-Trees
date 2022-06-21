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
#include "../src/lock.h"
#include "../src/BenchHelper.hpp"

// template<typename VT>
void vsize_parallel_insert_items(VSBTreeThread<uint64_t> vspft, m512i_arr items[], uint64_t vals[], size_t num) {
    for(size_t i = 0; i < num; i++) {
        // m512i_arr tmp(&items[i], 2);
        vspft.insert(items[i], vals[i]);
        // cout << "INDFS" << endl;
    }
}

// template<typename VT>
void vsize_parallel_pq_items(VSBTreeThread<uint64_t> vspft, m512i_arr items[], uint64_t vals[], size_t num) {
    for(size_t i = 0; i < num; i++) {
        // m512i_arr tmp(&items[i], 2);
        std::pair<uint64_t, bool> test = vspft.pquery(items[i]);
        // cout << i << " " << test.first << " " << test.second << endl;
        assert(test.second && (test.first == vals[i]));
    }
}

int main(int argc, char** argv) {
    unsigned seed = chrono::steady_clock::now().time_since_epoch().count();
    mt19937 generator (seed);
    // vsize_parallel_fusion_b_node<uint64_t>* root = new vsize_parallel_fusion_b_node<uint64_t>();

    // rw_lock_init(&root->mtx);

    size_t bigtestsize = 30;
    if(argc >= 2)
        bigtestsize = atoi(argv[1]);
    size_t numThreads = 1;
    if(argc >= 3)
        numThreads = atoi(argv[2]);

    VariableSizeParallelFusionBTree<uint64_t> tree{numThreads};
    
    __m512i* big_randomlist = static_cast<__m512i*>(std::aligned_alloc(64, bigtestsize*64));
    uint64_t* vals = new uint64_t[bigtestsize];
    for(size_t i{0}; i < bigtestsize; i++) {
    	big_randomlist[i] = gen_random_vec(generator);
        // vals[i] = gen_random_uint64(generator);
        vals[i] = i;
    }
    m512i_arr* test_arr = static_cast<m512i_arr*>(malloc(sizeof(m512i_arr)*bigtestsize)); //why new no work here?
    for(size_t i{0}, j=0; i < bigtestsize; i++) {
        if(i%10 == 0) {
            j = i;
            test_arr[i] = m512i_arr(1);
            test_arr[i].push_back(big_randomlist[i]);
        }
        else {
            test_arr[i] = m512i_arr(2);
            test_arr[i].push_back(big_randomlist[j]);
            test_arr[i].push_back(big_randomlist[i]);
        }
    }
    free(big_randomlist);

    std::vector<size_t> indices;
    std::vector<VSBTreeThread<uint64_t>> vspfts;
    for(size_t i{0}; i <= numThreads; i++) {
        indices.push_back(bigtestsize*i/numThreads);
        vspfts.push_back(VSBTreeThread<uint64_t>(tree, i));
    }

    shuffle(test_arr, test_arr+bigtestsize, generator);

    BenchHelper bench(numThreads);

    for(size_t i{0}; i < numThreads; i++) {
        bench.addFunctionForThreadTest([=] () -> void { vsize_parallel_insert_items(vspfts[i], test_arr+indices[i], vals+indices[i], indices[i+1]-indices[i]);});
    }
    bench.timeThreadedFunction("vsize parallel insert");

    for(size_t i{0}; i < numThreads; i++) {
        bench.addFunctionForThreadTest([=] () -> void { vsize_parallel_pq_items(vspfts[i], test_arr+indices[i], vals+indices[i], indices[i+1]-indices[i]);});
    }
    bench.timeThreadedFunction("vsize parallel point query random order");

    // start = chrono::high_resolution_clock::now();
    // sort(big_randomlist, big_randomlist+bigtestsize, fast_compare__m512i);
    // end = chrono::high_resolution_clock::now();
    // duration = chrono::duration_cast<chrono::microseconds>(end-start);
    // cout << "Time to sort with std::sort: " << duration.count() << endl;
}