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

uint64_t gen_random_uint64(mt19937& generator) {
    uint64_t A;
    std::uniform_int_distribution<uint64_t> temporary_distribution(0, ULLONG_MAX);
    A = temporary_distribution(generator);
    return A;
}

__m512i gen_random_vec(mt19937& generator) {
    __m512i A;
    std::uniform_int_distribution<uint64_t> temporary_distribution(0, ULLONG_MAX);
    for(int i=0; i < 8; i++){
        A[i] = temporary_distribution(generator);
    }
    return A;
}

__m512i gen_random_vec_one_bit(mt19937& generator) {
    __m512i A;
    std::uniform_int_distribution<uint64_t> temporary_distribution1(0, 63);
    uint64_t posbyte = temporary_distribution1(generator);
    __mmask64 k = _cvtu64_mask64(1ull << posbyte);
    std::uniform_int_distribution<uint64_t> temporary_distribution2(0, 7);
    uint64_t posbit = temporary_distribution2(generator);
    A = _mm512_maskz_set1_epi8 (k, (1ull << posbit));
    return A;
}

vector<int> generate_random_positions(mt19937& generator, int count, int maxpos=511){
    set<int> tmp;
    std::uniform_int_distribution<uint64_t> temporary_distribution(0, maxpos);
    while(tmp.size() < count) {
        tmp.insert(temporary_distribution(generator));
    }
    vector<int> random_pos;
    for (std::set<int>::iterator it=tmp.begin(); it!=tmp.end(); ++it)
        random_pos.push_back(*it);
    shuffle(random_pos.begin(), random_pos.end(), generator);
    return random_pos;
}

__m512i gen_vec_one_bit(int bit_pos) {
    __m512i A;
    uint64_t posbyte = bit_pos/8;
    __mmask64 k = _cvtu64_mask64(1ull << posbyte);
    uint64_t posbit = bit_pos%8;
    A = _mm512_maskz_set1_epi8 (k, (1ull << posbit));
    return A;
}

__m512i gen_random_vec_all_but_one_bit(mt19937& generator) {
    return _mm512_andnot_si512(gen_random_vec_one_bit(generator), _mm512_set1_epi8(255));
}

void print_node_info(fusion_node& test_node) {
    cout << "Extraction mask:" << endl;
    print_binary_uint64(test_node.tree.byte_extract, true);
    print_binary_uint64(test_node.tree.bitextract[0], false, 8);
    cout << " ";
    print_binary_uint64(test_node.tree.bitextract[1], true, 8);
    cout << "Tree and ignore bits:" << endl;
    print_vec(test_node.tree.treebits, true, 16);
    print_vec(test_node.ignore_mask, true, 16);
    cout << "Sorted to real positions:" << endl;
    print_vec(test_node.key_positions, true, 8);
}

int main(){
    unsigned seed = chrono::steady_clock::now().time_since_epoch().count();
    mt19937 generator (seed);

    //return 0;
    
    constexpr long long bigtestsize = 10000000;
    __m512i* big_randomlist = static_cast<__m512i*>(std::aligned_alloc(64, bigtestsize*64));
    uint64_t* small_randomlist = (uint64_t*)malloc(bigtestsize*sizeof(uint64_t));
    set<uint64_t> list_set;
    boost::container::set<uint64_t> boost_set;
    FusionBTree ft();
    for(int i=0; i < bigtestsize; i++) {
    	big_randomlist[i] = gen_random_vec(generator);
        small_randomlist[i] = gen_random_uint64(generator);
    }
    auto start = chrono::high_resolution_clock::now();
    // SimpleAlloc<fusion_b_node, 64> allocator(bigtestsize/8); //gotta be a bit more efficient here but whatever lol
    for(int i=0; i < bigtestsize; i++) {
    	ft.insert(big_randomlist[i]);
    }
    cout << "random seed is " << seed << endl;
    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::microseconds>(end-start);
    cout << "Time to insert: " << duration.count() << endl;
    
    start = chrono::high_resolution_clock::now();
    for(int i=0; i < bigtestsize; i++) {
    	list_set.insert(small_randomlist[i]);
    }
    end = chrono::high_resolution_clock::now();
    duration = chrono::duration_cast<chrono::microseconds>(end-start);
    cout << "Time to insert 64 bit ints to std::set: " << duration.count() << endl;

    start = chrono::high_resolution_clock::now();
    for(int i=0; i < bigtestsize; i++) {
    	boost_set.insert(small_randomlist[i]);
    }
    end = chrono::high_resolution_clock::now();
    duration = chrono::duration_cast<chrono::microseconds>(end-start);
    cout << "Time to insert 64 bit ints to boost::container::set: " << duration.count() << endl;

    __m512i* prev2;
    uint64_t cNull = 0;
    start = chrono::high_resolution_clock::now();
    for(int i=0; i < bigtestsize; i++) {
        prev2 = ft.successor(big_randomlist[i]);
        if(prev2 == NULL) cNull++;
        //assert(first_diff_bit_pos(prev2, big_randomlist[i]) == -1);
    }
    end = chrono::high_resolution_clock::now();
    duration = chrono::duration_cast<chrono::microseconds>(end-start);
    cout << "Time to query nodes in random order: " << duration.count() << endl;

    start = chrono::high_resolution_clock::now();
    sort(big_randomlist, big_randomlist+bigtestsize, fast_compare__m512i);
    end = chrono::high_resolution_clock::now();
    duration = chrono::duration_cast<chrono::microseconds>(end-start);
    cout << "Time to sort with std::sort: " << duration.count() << endl;

    for(size_t i = 0; i < bigtestsize; i++) {
        std::uniform_int_distribution<uint64_t> temporary_distribution(0, bigtestsize-1);
        size_t j = temporary_distribution(generator);
        swap(big_randomlist[i], big_randomlist[j]);
    }

    start = chrono::high_resolution_clock::now();
    FusionQSort(big_randomlist, bigtestsize);
    end = chrono::high_resolution_clock::now();
    duration = chrono::duration_cast<chrono::microseconds>(end-start);
    cout << "Time to sort with FusionQSort: " << duration.count() << endl;

    for(size_t i = 0; i < bigtestsize-1; i++) {
        assert(first_diff_bit_pos(big_randomlist[i+1], big_randomlist[i]) != -1);
        // assert(compare__m512i(big_randomlist[i], big_randomlist[i+1]) == fast_compare__m512i(big_randomlist[i], big_randomlist[i+1]));
        if(!compare__m512i(big_randomlist[i], big_randomlist[i+1])) {
            cout << "Failed at " << i << endl;
            exit(1);
        }
    }
    
    __m512i prev = {0};
    start = chrono::high_resolution_clock::now();
    for(int i=0; i < bigtestsize; i++) {
    	prev = *ft.successor(prev);
    	// assert(first_diff_bit_pos(prev, big_randomlist[i]) == -1);
    }
    end = chrono::high_resolution_clock::now();
    duration = chrono::duration_cast<chrono::microseconds>(end-start);
    cout << "Time to retrieve sorted list with fusion tree: " << duration.count() << endl;


    for(int i=0; i < bigtestsize-1; i++) {
    	__m512i* treesucc = ft.successor(root, big_randomlist[i]);
    	if(treesucc == NULL){
    		cout << "treesuc is NULL" << endl;
            break;
    	}
    	else {
    		if(first_diff_bit_pos(*treesucc, big_randomlist[i+1]) != -1) {
    			cout << "Wrong at " << i << endl;
    			print_binary_uint64_big_endian((*treesucc)[7], true, 64, 8);
    			break;
		    }
    	}
    }

    // ft.printTree();
    cout << "Num Nodes: " << ft.numNodes() << endl;
    cout << "Total Depth: " << ft.totalDepth() << endl;
    cout << "Mem usage: " << ft.memUsage() << endl;
    cout << "max depth: " << ft.maxDepth() << endl;
    cout << "random seed is " << seed << endl;
}

