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

#include "../src/SimpleAlloc.h"
#include "../src/fusion_tree.h"
#include "../src/HelperFuncs.h"
#include "../src/FusionBTree.h"
#include "../src/FusionQSort.h"
#include "../src/BenchHelper.hpp"

int main(int argc, char** argv){
    unsigned seed = chrono::steady_clock::now().time_since_epoch().count();
    mt19937 generator (seed);

    BenchHelper bench;
     
    size_t bigtestsize = 30;
    if(argc >= 2)
        bigtestsize = atoi(argv[1]);
    __m512i* big_randomlist = static_cast<__m512i*>(std::aligned_alloc(64, bigtestsize*64));
    uint64_t* small_randomlist = (uint64_t*)malloc(bigtestsize*sizeof(uint64_t));
    set<uint64_t> list_set;
    boost::container::set<uint64_t> boost_set;
    FusionBTree ft;
    for(size_t i{0}; i < bigtestsize; i++) {
    	big_randomlist[i] = gen_random_vec(generator);
        small_randomlist[i] = gen_random_uint64(generator);
    }

    bench.timeFunction([&] ()-> void {
        for(size_t i{0}; i < bigtestsize; i++) {
            ft.insert(big_randomlist[i]);
        }
    }, "fusion insert");
    
    bench.timeFunction([&] () -> void {
        for(size_t i{0}; i < bigtestsize; i++) {
            list_set.insert(small_randomlist[i]);
        }
    }, "std::set insert");
    
    bench.timeFunction([&] ()-> void {
        for(size_t i{0}; i < bigtestsize; i++) {
            boost_set.insert(small_randomlist[i]);
        }
    }, "boost::container::set insert");

    bench.timeFunction([&] ()-> void {
        for(size_t i{0}; i < bigtestsize; i++) {
            (void)*ft.successor(big_randomlist[i]);
            // assert(first_diff_bit_pos(prev, big_randomlist[i]) == -1);
        }
    }, "successor query on unsorted list");

    bench.timeFunction([&] ()-> void {
        sort(big_randomlist, big_randomlist+bigtestsize, fast_compare__m512i);
    }, "sort on big keys");

    for(size_t i{0}; i < bigtestsize; i++) {
        std::uniform_int_distribution<uint64_t> temporary_distribution(0, bigtestsize-1);
        size_t j = temporary_distribution(generator);
        swap(big_randomlist[i], big_randomlist[j]);
    }
    
    bench.timeFunction([&] ()-> void{
        FusionQSort(big_randomlist, bigtestsize);
    }, "FusionQSort on big keys");

    for(size_t i{0}; i < bigtestsize-1; i++) {
        assert(first_diff_bit_pos(big_randomlist[i+1], big_randomlist[i]) != -1);
        // assert(compare__m512i(big_randomlist[i], big_randomlist[i+1]) == fast_compare__m512i(big_randomlist[i], big_randomlist[i+1]));
        if(!compare__m512i(big_randomlist[i], big_randomlist[i+1])) {
            cout << "Failed at " << i << endl;
            exit(1);
        }
    }
    
    bench.timeFunction([&] ()-> void {
        __m512i prev = {0};
        for(size_t i{0}; i < bigtestsize; i++) {
            prev = *ft.successor(prev);
            assert(first_diff_bit_pos(prev, big_randomlist[i]) == -1);
        }
    }, "successor query on sorted list");
}

