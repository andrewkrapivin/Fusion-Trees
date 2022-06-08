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
#include "../src/VariableSizeFusionBTree.h"
#include "../src/FusionQSort.h"
#include "../src/lock.h"
#include "../src/BenchHelper.h"

void parallel_insert_items(parallel_fusion_b_node* root, __m512i items[], size_t num, string path, uint8_t id) {
    // cout << "path is: " << path << endl;
    // ofstream fout(path);
    ParallelFusionBTree pft(root, id);
    for(size_t i = 0; i < num; i++) {
        pft.insert(items[i]);
    }
    // fout.close();
}

template<bool chk = false>
void parallel_succ_items(parallel_fusion_b_node* root, __m512i items[], size_t num, string path, uint8_t id) {
    // cout << "path is: " << path << endl;
    // ofstream fout(path);
    ParallelFusionBTree pft(root, id);
    for(size_t i = 0; i < num; i++) {
        __m512i* test = pft.successor(items[i]);
        if constexpr (chk) assert(i >=num-2 || first_diff_bit_pos(*test, items[i+1]) == -1);
    }
    // fout.close();
}

void parallel_pred_items(parallel_fusion_b_node* root, __m512i items[], size_t num, string path, uint8_t id) {
    // cout << "path is: " << path << endl;
    // ofstream fout(path);
    ParallelFusionBTree pft(root, id);
    for(size_t i = 0; i < num; i++) {
        __m512i* test = pft.predecessor(items[i]);
        // assert(i == 0 || first_diff_bit_pos(*test, items[i-1]) == -1);
    }
    // fout.close();
}

int main(int argc, char** argv) {
    unsigned seed = chrono::steady_clock::now().time_since_epoch().count();
    mt19937 generator (seed);
    parallel_fusion_b_node* root = new parallel_fusion_b_node();
    // rw_lock_init(&root->mtx);

    size_t bigtestsize = 30;
    if(argc >= 2)
        bigtestsize = atoi(argv[1]);
    size_t numThreads = 1;
    if(argc >= 3)
        numThreads = atoi(argv[2]);
    if(argc < 3+numThreads) {
        exit(1);
    }
    
    __m512i* big_randomlist = static_cast<__m512i*>(std::aligned_alloc(64, bigtestsize*64));
    for(int i=0; i < bigtestsize; i++) {
    	big_randomlist[i] = gen_random_vec(generator);
    }

    std::vector<size_t> indices;
    for(size_t i = 0; i <= numThreads; i++) {
        indices.push_back(bigtestsize*i/numThreads);
    }

    BenchHelper bench(numThreads);

    for(size_t i = 0; i < numThreads; i++) {
        bench.addFunctionForThreadTest([=] () -> void { parallel_insert_items(root, big_randomlist+indices[i], indices[i+1]-indices[i], argv[3+i], i);});
    }
    bench.timeThreadedFunction("parallel insert");

    bench.timeFunction([&] ()-> void {
        sort(big_randomlist, big_randomlist+bigtestsize, fast_compare__m512i);
    }, "sort on big keys");

    for(size_t i = 0; i < numThreads; i++) {
        bench.addFunctionForThreadTest([=] () -> void { parallel_succ_items<true>(root, big_randomlist+indices[i], indices[i+1]-indices[i], argv[3+i], i);});
    }
    bench.timeThreadedFunction("parallel successor sorted");

    shuffle(big_randomlist, big_randomlist+bigtestsize, generator);

    for(size_t i = 0; i < numThreads; i++) {
        bench.addFunctionForThreadTest([=] () -> void { parallel_succ_items(root, big_randomlist+indices[i], indices[i+1]-indices[i], argv[3+i], i);});
    }
    bench.timeThreadedFunction("parallel successor random");


    for(size_t i = 0; i < numThreads; i++) {
        bench.addFunctionForThreadTest([=] () -> void { parallel_pred_items(root, big_randomlist+indices[i], indices[i+1]-indices[i], argv[3+i], i);});
    }
    bench.timeThreadedFunction("parallel predecessor random");
}