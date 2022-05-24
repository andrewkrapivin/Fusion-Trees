#ifndef VSIZE_FUSION_B_TREE_H_INCLUDED
#define VSIZE_FUSION_B_TREE_H_INCLUDED

#include "fusion_tree.h"
#include "SimpleAlloc.h"
#include "UpgradeableMutex.h"
#include "lock.h"
#include "BTreeHelper.h"
#include <thread>
#include <fstream>
#include <ostream>

template<typename VT>
struct vsize_parallel_fusion_b_node {
    fusion_node fusion_internal_tree;
	vsize_parallel_fusion_b_node* children[MAX_FUSION_SIZE+1];
    ReaderWriterLock mtx;
    VT vals[MAX_FUSION_SIZE];
    vsize_parallel_fusion_b_node* subtree_roots[MAX_FUSION_SIZE];
    //Not sure how to do the skip_nodes stuff. Maybe just do that with the hash tables? Or actually wait is there even any reason for that

    vsize_parallel_fusion_b_node();
    ~vsize_parallel_fusion_b_node();
    void set_val(VT val, int pos);
    bool has_val(int pos);
};

//Size of things is really going out of control. But also the RWlocks have a lot of data
//So maybe at lower levels (how to define these?) have simple exclusive locks since not a lot of cores access it to reduce memory overhead?
// template<typename VT>
// struct vsize_parallel_fusion_b_leaf {
//     fusion_node fusion_internal_tree;
// 	vsize_parallel_fusion_b_node* children[MAX_FUSION_SIZE+1];
//     ReaderWriterLock mtx;
//     ReaderWriterLock mtx2;
//     vsize_parallel_fusion_b_leaf* skip_nodes[MAX_FUSION_SIZE*2];
//     VT* vals[MAX_FUSION_SIZE];
// };

struct m512i_arr {
    static constexpr int default_size = 1;
    __m512i* ptr;
    int size=0, maxsize=default_size;
    int offset=0;

    m512i_arr();
    m512i_arr(int msize);
    m512i_arr(__m512i* ptr, int size): ptr(ptr), size(size), maxsize(size), offset(0) {};
    m512i_arr(const m512i_arr& arr): ptr(arr.ptr), size(arr.size), maxsize(arr.maxsize), offset(arr.offset) {};
    void push_back(__m512i val);
    void free();
    __m512i* get(int pos); //add overloading stuff to make it easier with []
    __m512i* get_at_offset();
    bool inc_offset();
};

template<typename VT>
using VSP_BTState = BTState<vsize_parallel_fusion_b_node<VT>>;

template<typename VT>
class VariableSizeParallelFusionBTree {
    private:
        vsize_parallel_fusion_b_node<VT>* root;
        int thread_id;
        std::pair<VT, bool> new_root_pquery(VSP_BTState<VT> state, m512i_arr key, int branch);
        std::pair<VT, bool> pquery(m512i_arr key, vsize_parallel_fusion_b_node<VT>* subroot);

    public:
        VariableSizeParallelFusionBTree(vsize_parallel_fusion_b_node<VT>* root, int thread_id): root(root), thread_id(thread_id) {}
        void insert(m512i_arr key, VT val);
        std::pair<VT, bool> pquery(m512i_arr key); //returning VT and whether that is actually a valid value, ie whether the key is in the database
        m512i_arr successor(m512i_arr key);
        m512i_arr predecessor(m512i_arr key);
};

#endif
