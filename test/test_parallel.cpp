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
    /*cout << "Generated random bit at byte " << posbyte << " and bit " << posbit << endl;
    print_binary_uint64(_cvtmask64_u64(k), true);
    print_binary_uint64((1ull << posbit), true);
    print_vec(A, true, 8);*/
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
    /*cout << "Generated random bit at byte " << posbyte << " and bit " << posbit << endl;
    print_binary_uint64(_cvtmask64_u64(k), true);
    print_binary_uint64((1ull << posbit), true);
    print_vec(A, true, 8);*/
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

void parallel_insert_items(fusion_b_node* root, __m512i items[], size_t num, string path, uint8_t id) {
    cout << "path is: " << path << endl;
    ofstream fout(path);
    for(size_t i = 0; i < num; i++) {
        // printTree(root);
        // cout << "Inserting element " << i << endl;
        parallel_insert_full_tree_DLock(root, items[i], fout, id);
    }
    fout.close();
}

int main(int argc, char** argv) {
    unsigned seed = chrono::steady_clock::now().time_since_epoch().count();
    mt19937 generator (2);
    fusion_b_node* root = new fusion_b_node();
    rw_lock_init(&root->mtx);

    size_t bigtestsize = 30;
    if(argc >= 2)
        bigtestsize = atoi(argv[1]);
    size_t numThreads = 1;
    if(argc >= 3)
        numThreads = atoi(argv[2]);
    if(argc < 3+numThreads) {
        exit(1);
    }
    // vector<ofstream> actual_files(argc >= 3+numThreads ? numThreads : 0);
    // vector<ostream> files(numThreads);
    // if(argc >= 3+numThreads) {
    //     for(int i=0; i = numThreads; i++) {
    //         actual_files[i].open(argv[3+i]);
    //         ostream f(&actual_files[i]);
    //         files[i] = f;
    //     }
    // }
    // else {
    //     for(int i=1; i <= numThreads; i++) {
    //         files[i].open(&cout);
    //     }
    // }
    __m512i* big_randomlist = static_cast<__m512i*>(std::aligned_alloc(64, bigtestsize*64));
    for(int i=0; i < bigtestsize; i++) {
    	big_randomlist[i] = gen_random_vec(generator);
    }

    std::vector<size_t> indices;
    for(size_t i = 0; i <= numThreads; i++) {
        indices.push_back(bigtestsize*i/numThreads);
    }

    auto start = chrono::high_resolution_clock::now();
    //Figure out how to test this
    std::vector<std::thread> threads;
    for(size_t i = 0; i < numThreads; i++) {
        // threads.push_back(std::thread(parallel_insert_items, root, big_randomlist+indices[i], indices[i+1]-indices[i], argv[3+i]));
        threads.push_back(std::thread(parallel_insert_items, root, big_randomlist+indices[i], indices[i+1]-indices[i], argv[3+i], i));
    }

    for(auto& th: threads) {
        th.join();
    }

    cout << "random seed is " << seed << endl;
    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::microseconds>(end-start);
    cout << "Time to insert: " << duration.count() << endl;

    // start = chrono::high_resolution_clock::now();
    // SimpleAlloc<fusion_b_node, 64> allocator(bigtestsize/8); //gotta be a bit more efficient here but whatever lol
    // fusion_b_node* root2 = new_empty_node(allocator);
    // for(int i=0; i < bigtestsize; i++) {
    //     // cout << "i is: " << i << endl;
    //     //if(root!= NULL) printTree(root);
    // 	root2 = insert_full_tree(root2, big_randomlist[i], allocator);
    // }
    // // printTree(root);
    // cout << "random seed is " << seed << endl;
    // end = chrono::high_resolution_clock::now();
    // duration = chrono::duration_cast<chrono::microseconds>(end-start);
    // cout << "Time to insert: " << duration.count() << endl;

    start = chrono::high_resolution_clock::now();
    sort(big_randomlist, big_randomlist+bigtestsize, fast_compare__m512i);
    end = chrono::high_resolution_clock::now();
    duration = chrono::duration_cast<chrono::microseconds>(end-start);
    cout << "Time to sort with std::sort: " << duration.count() << endl;

    __m512i prev = {0};
    start = chrono::high_resolution_clock::now();
    for(int i=0; i < bigtestsize; i++) {
    	prev = *successor(root, prev);
    	assert(first_diff_bit_pos(prev, big_randomlist[i]) == -1);
    }
    end = chrono::high_resolution_clock::now();
    duration = chrono::duration_cast<chrono::microseconds>(end-start);
    cout << "Time to retrieve sorted list with fusion tree: " << duration.count() << endl;
}