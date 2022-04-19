#ifndef FUSION_B_TREE_H_INCLUDED
#define FUSION_B_TREE_H_INCLUDED

#include "fusion_tree.h"
#include "SimpleAlloc.h"
#include "UpgradeableMutex.h"
#include "lock.h"
#include <thread>
#include <fstream>
#include <ostream>

//maybe do like numbranches macro defined as max_fusion_size+1 to make things a bit nicer?

typedef struct fusion_b_node{
	fusion_node fusion_internal_tree;
	fusion_b_node* children[MAX_FUSION_SIZE+1];
    fusion_b_node* parent;
    bool visited;
    uint64_t id;
} fusion_b_node;

typedef struct parallel_fusion_b_node {
    fusion_node fusion_internal_tree;
	parallel_fusion_b_node* children[MAX_FUSION_SIZE+1];
    parallel_fusion_b_node* parent;
    ReaderWriterLock mtx;
}parallel_fusion_b_node;

class FusionBTree {
    private:
        fusion_b_node* root;

    public:
        FusionBTree();
        void insert(__m512i key);
        __m512i* successor(__m512i key);
        __m512i* predecessor(__m512i key);
        void printTree();
        int maxDepth();
        size_t numNodes();
        size_t totalDepth();
        size_t memUsage();
};

class ParallelFusionBTree {
    private:
        parallel_fusion_b_node* root;
        int thread_id;

    public:
        ParallelFusionBTree(parallel_fusion_b_node* root, int thread_id): root(root), thread_id(thread_id) {}
        void insert(__m512i key);
        __m512i* successor(__m512i key);
        __m512i* predecessor(__m512i key);
};

#endif
