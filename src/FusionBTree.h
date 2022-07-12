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
#include "HashLocks.hpp"
// #include "ThreadedIdGenerator.hpp"

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

typedef struct ParallelFusionBNode {
    fusion_node fusion_internal_tree;
	ParallelFusionBNode* children[MAX_FUSION_SIZE+1];
    // ReaderWriterLock mtx;
    // ReadWriteMutex mtx;
    // HashMutex mtx; //todo!!

    // ParallelFusionBNode(size_t numThreads);
    // ParallelFusionBNode(LockHashTable* table, size_t id);
    ParallelFusionBNode();
    // ~ParallelFusionBNode();
} ParallelFusionBNode;

// Implementation question: how to make these two data structures the same size? Since they have exactly the same data?
// That is, how to make it so that like adding padding doesn't happen until the "end," only in the final data structure?
// typedef struct pnodetest1 {
//     __m512i keys[MAX_FUSION_SIZE];
// 	fusion_tree tree;
// 	__m256i ignore_mask;
// 	__m128i key_positions;
// 	parallel_fusion_b_node* children[MAX_FUSION_SIZE+1];
// } pnodetest1;

// typedef struct pnodetest2 {
//     fusion_node fusion_internal_tree;
// 	parallel_fusion_b_node* children[MAX_FUSION_SIZE+1];
// } pnodetest2;

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
        size_t numThreads;
        StripedLockTable lockTable;
        // ThreadedIdGenerator idGen;
        ParallelFusionBNode root;
        vector<ofstream> debugFiles;
        // size_t thread_id;

    public:
        // ParallelFusionBTree(parallel_fusion_b_node* root, size_t numThreads, size_t thread_id): root(root), numThreads{numThreads}, thread_id(thread_id) {}
        ParallelFusionBTree(size_t numThreads);
        void insert(__m512i key, size_t threadId);
        __m512i* successor(__m512i key, size_t threadId);
        __m512i* predecessor(__m512i key, size_t threadId);
};

//Feels like a bad name but just meant to run the functions in ParallelFusionBTree with the threadId inside the class
class ParallelFusionBTreeThread {
    private:
        ParallelFusionBTree& tree;
        size_t threadId;
    
    public:
        ParallelFusionBTreeThread(ParallelFusionBTree& tree, size_t threadId);
        void insert(__m512i key);
        __m512i* successor(__m512i key);
        __m512i* predecessor(__m512i key);
};

#endif
