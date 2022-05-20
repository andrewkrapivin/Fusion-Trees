#ifndef VSIZE_FUSION_B_TREE_H_INCLUDED
#define VSIZE_FUSION_B_TREE_H_INCLUDED

#include "fusion_tree.h"
#include "SimpleAlloc.h"
#include "UpgradeableMutex.h"
#include "lock.h"
#include <thread>
#include <fstream>
#include <ostream>

typedef struct vsize_parallel_fusion_b_node {
    fusion_node fusion_internal_tree;
	vsize_parallel_fusion_b_node* children[MAX_FUSION_SIZE+1];
    ReaderWriterLock mtx;
} vsize_parallel_fusion_b_node;

//Size of things is really going out of control. But also the RWlocks have a lot of data
//So maybe at lower levels (how to define these?) have simple exclusive locks since not a lot of cores access it to reduce memory overhead?
template<typename VT>
struct vsize_parallel_fusion_b_leaf {
    fusion_node fusion_internal_tree;
	vsize_parallel_fusion_b_node* children[MAX_FUSION_SIZE+1];
    ReaderWriterLock mtx;
    ReaderWriterLock mtx2;
    vsize_parallel_fusion_b_leaf* skip_nodes[MAX_FUSION_SIZE*2];
    VT* vals[MAX_FUSION_SIZE];
};

template<typename VT>
class VariableSizeParallelFusionBTree {
    private:
        vsize_parallel_fusion_b_node* root;
        int thread_id;

    public:
        VariableSizeParallelFusionBTree(vsize_parallel_fusion_b_node* root, int thread_id): root(root), thread_id(thread_id) {}
        void insert(__m512i key);
        __m512i* successor(__m512i key);
        __m512i* predecessor(__m512i key);
};

#endif
