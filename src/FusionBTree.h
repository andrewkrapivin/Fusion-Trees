#ifndef FUSION_B_TREE_H_INCLUDED
#define FUSION_B_TREE_H_INCLUDED

#include "fusion_tree.h"
#include "SimpleAlloc.h"
// #include "lock.h"
#include "Locks.hpp"
#include <thread>
#include <fstream>
#include <ostream>
#include <immintrin.h>

//maybe do like numbranches macro defined as max_fusion_size+1 to make things a bit nicer?

//Make the fusion_internal_tree packed 
typedef struct fusion_b_node {
	fusion_node fusion_internal_tree;
	fusion_b_node* children[MAX_FUSION_SIZE+1];
    // fusion_b_node* parent;
    // bool visited;
    // uint64_t id;

    fusion_b_node();
} fusion_b_node;

typedef struct parallel_fusion_b_node {
    fusion_node fusion_internal_tree;
	parallel_fusion_b_node* children[MAX_FUSION_SIZE+1];
    // ReaderWriterLock mtx;
    ReadWriteMutex mtx;

    parallel_fusion_b_node(size_t numThreads);
    ~parallel_fusion_b_node();
} parallel_fusion_b_node;

// Implementation question: how to make these two data structures the same size? Since they have exactly the same data?
// That is, how to make it so that like adding padding doesn't happen until the "end," only in the final data structure?
typedef struct pnodetest1 {
    __m512i keys[MAX_FUSION_SIZE];
	fusion_tree tree;
	__m256i ignore_mask;
	__m128i key_positions;
	parallel_fusion_b_node* children[MAX_FUSION_SIZE+1];
} pnodetest1;

typedef struct pnodetest2 {
    fusion_node fusion_internal_tree;
	parallel_fusion_b_node* children[MAX_FUSION_SIZE+1];
} pnodetest2;

class FusionBTree {
    private:
        fusion_b_node* root;

    public:
        FusionBTree();
        void insert(__m512i key);
        __m512i* successor(__m512i key);
        __m512i* predecessor(__m512i key);
};

class ParallelFusionBTree {
    private:
        parallel_fusion_b_node* root;
        size_t numThreads;
        size_t thread_id;

    public:
        ParallelFusionBTree(parallel_fusion_b_node* root, size_t numThreads, size_t thread_id): root(root), numThreads{numThreads}, thread_id(thread_id) {}
        void insert(__m512i key);
        __m512i* successor(__m512i key);
        __m512i* predecessor(__m512i key);
};

#endif
