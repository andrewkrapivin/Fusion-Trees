#ifndef VSIZE_FUSION_B_TREE_H_INCLUDED
#define VSIZE_FUSION_B_TREE_H_INCLUDED

#include "fusion_tree.h"
#include "SimpleAlloc.h"
// #include "lock.h"
#include "Locks.hpp"
#include "BTreeHelper.hpp"
#include "HelperFuncs.h"
#include "ThreadedIdGenerator.hpp"
#include "HashLocks.hpp"
#include <cstring>
#include <thread>
#include <fstream>
#include <ostream>

int idc=0;

template<typename VT>
struct vsize_parallel_fusion_b_node {
    fusion_node fusion_internal_tree;
	vsize_parallel_fusion_b_node* children[MAX_FUSION_SIZE+1];
    // ReaderWriterLock mtx;
    ReadWriteMutex mtx;
    VT vals[MAX_FUSION_SIZE];
    vsize_parallel_fusion_b_node* subtree_roots[MAX_FUSION_SIZE];
    //Not sure how to do the skip_nodes stuff. Maybe just do that with the hash tables? Or actually wait is there even any reason for that
    // int id; //temp debug

    vsize_parallel_fusion_b_node(size_t numThreads);
    ~vsize_parallel_fusion_b_node();
    void deleteSubtrees();
    void ins_val(VT val, int pos);
    void ins_subtree(vsize_parallel_fusion_b_node<VT>* root, int pos);
    bool has_val(int pos);
    void set_val(VT val, int pos);
    void finish_key_insert(int pos);
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

//Really badly coded rn. Def fix
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
        size_t numThreads;
        vsize_parallel_fusion_b_node<VT> root;
        ThreadedIdGenerator idgen;
        LockHashTable locks;
        void set_val_and_new_root(VSP_BTState<VT> state, m512i_arr key, VT val, int branch, size_t thread_id);
        void insert(vsize_parallel_fusion_b_node<VT>* root, m512i_arr key, VT val, size_t threadId);
        std::pair<VT, bool> new_root_pquery(VSP_BTState<VT> state, m512i_arr key, int branch, size_t threadId);
        std::pair<VT, bool> pquery(m512i_arr key, vsize_parallel_fusion_b_node<VT>* subroot, size_t threadId);

    public:
        VariableSizeParallelFusionBTree(size_t numThreads);
        ~VariableSizeParallelFusionBTree();
        void insert(m512i_arr key, VT val, size_t threadId) {
            insert(&root, key, val, threadId);
        }
        //returning VT and whether that is actually a valid value, ie whether the key is in the database
        std::pair<VT, bool> pquery(m512i_arr key, size_t threadId) {
            return pquery(key, &root, threadId);
        }
        // m512i_arr successor(m512i_arr key, size_t thread);
        // m512i_arr predecessor(m512i_arr key, size_t thread);

};

template<typename VT>
class VSBTreeThread {
    private:
        VariableSizeParallelFusionBTree<VT>& tree;
        size_t threadId;
        // std::pair<VT, bool> new_root_pquery(VSP_BTState<VT> state, m512i_arr key, int branch);
        // std::pair<VT, bool> pquery(m512i_arr key, vsize_parallel_fusion_b_node<VT>* subroot);
        

    public:
        VSBTreeThread(VariableSizeParallelFusionBTree<VT>& tree, int threadId): tree(tree), threadId(threadId) {}
        void insert(m512i_arr key, VT val) {
            tree.insert(key, val, threadId);
        }
        std::pair<VT, bool> pquery(m512i_arr key) {
            return tree.pquery(key, threadId);
        }
        // m512i_arr successor(m512i_arr key) {
        //     return tree.successor(key, threadId);
        // }
        // m512i_arr predecessor(m512i_arr key) {
        //     return tree.predecessor(key, threadId);   
        // }
};

void split_high_low(uint16_t mask, uint16_t& low, uint16_t& high, int pos) {
    uint16_t lowbits_mask = (1 << pos) - 1;
    low = mask & lowbits_mask;
    high = mask & (~lowbits_mask);
}

uint16_t shift_part_left(uint16_t mask, int pos) {
    uint16_t lowbits, highbits;
    split_high_low(mask, lowbits, highbits, pos);
    return (highbits << 1) + lowbits;

}

template<typename VT>
vsize_parallel_fusion_b_node<VT>::vsize_parallel_fusion_b_node(size_t numThreads): fusion_internal_tree(), mtx{numThreads} {
    for(int i=0; i<MAX_FUSION_SIZE+1; i++) {
        children[i] = NULL;
    }
    for(int i=0; i<MAX_FUSION_SIZE; i++) {
        vals[i] = 0;
    }
    for(int i=0; i<MAX_FUSION_SIZE; i++) {
        subtree_roots[i] = NULL;
    }
    // rw_lock_init(&mtx);
    // id = idc++;
}

template<typename VT>
vsize_parallel_fusion_b_node<VT>::~vsize_parallel_fusion_b_node() {
    // pc_destructor(&mtx.pc_counter);
}

template<typename VT>
void vsize_parallel_fusion_b_node<VT>::deleteSubtrees() {
    for(size_t i{0}; i < MAX_FUSION_SIZE+1; i++) {
        if(children[i] != NULL) {
            children[i]->deleteSubtrees();
            delete children[i];
        }
    }
    for(size_t i{0}; i < MAX_FUSION_SIZE; i++) {
        if(subtree_roots[i] != NULL) {
            subtree_roots[i]->deleteSubtrees();
            delete subtree_roots[i];
        }
    }
}

template<typename VT>
void vsize_parallel_fusion_b_node<VT>::set_val(VT val, int pos) {
    vals[pos] = val;
    fusion_internal_tree.tree.meta.fullkey |= 1 << pos;
}

template<typename VT>
void vsize_parallel_fusion_b_node<VT>::ins_val(VT val, int pos) {
    // std::cout << val << ' ' << pos << std::endl;
    fusion_internal_tree.tree.meta.fullkey = shift_part_left(fusion_internal_tree.tree.meta.fullkey, pos);
    for(int i=MAX_FUSION_SIZE-1; i > pos ; i--) { // change MAX_FUSION_SIZE to fusion_internal_tree.tree.meta.size? Cause should be updated?
        vals[i] = vals[i-1];
    }
    set_val(val, pos);
    // std::cout << "Mask: " << fusion_internal_tree.tree.meta.fullkey << ", vals after ins: ";
    // for(int i=0; i < fusion_internal_tree.tree.meta.size; i++) {
    //     std::cout << vals[i] << ' ';
    // }
    // std::cout << std::endl;
}

template<typename VT>
void vsize_parallel_fusion_b_node<VT>::ins_subtree(vsize_parallel_fusion_b_node<VT>* root, int pos) {
    for(int i=MAX_FUSION_SIZE-1; i > pos ; i--) { // change MAX_FUSION_SIZE to fusion_internal_tree.tree.meta.size? Cause should be updated?
        subtree_roots[i] = subtree_roots[i-1];
    }
    subtree_roots[pos] = root;
}

template<typename VT>
void vsize_parallel_fusion_b_node<VT>::finish_key_insert(int pos) {
    // for(int i=MAX_FUSION_SIZE-1; i > pos ; i--) { // change MAX_FUSION_SIZE to fusion_internal_tree.tree.meta.size? Cause should be updated?
    //     subtree_roots[i] = subtree_roots[i-1];
    // }
    // subtree_roots[pos] = NULL;
    ins_subtree(NULL, pos);
    fusion_internal_tree.tree.meta.fullkey = shift_part_left(fusion_internal_tree.tree.meta.fullkey, pos);
    for(int i=MAX_FUSION_SIZE-1; i > pos ; i--) { // change MAX_FUSION_SIZE to fusion_internal_tree.tree.meta.size? Cause should be updated?
        vals[i] = vals[i-1];
    }
}


template<typename VT>
bool vsize_parallel_fusion_b_node<VT>::has_val(int pos) {
    return fusion_internal_tree.tree.meta.fullkey & (1 << pos);
}


//Sus constructor cause not destructed that way? idk
m512i_arr::m512i_arr(): m512i_arr{default_size} {
}

m512i_arr::m512i_arr(int msize) {
    maxsize = msize;
    ptr = static_cast<__m512i*> (std::aligned_alloc(64, 64*maxsize));
    size = 0;
    offset = 0;
}

void m512i_arr::push_back(__m512i val) {
    if(maxsize < 0) return;
    if(size < maxsize) {
        ptr[size++] = val;
    }
    else {
        //kinda meh
        maxsize*=2;
        __m512i* nptr = static_cast<__m512i*> (std::aligned_alloc(64, 64*maxsize));
        memcpy(nptr, ptr, 64*maxsize);
        std::free(ptr);
        ptr = nptr;
    }
}

void m512i_arr::free() {
    std::free(ptr);
    maxsize = -1;
}

__m512i* m512i_arr::get(int pos) {
    if(pos < 0 || pos >= size) return NULL;
    return &ptr[pos];
}

__m512i* m512i_arr::get_at_offset() {
    if(offset < 0 || offset >= size) return NULL;
    return &ptr[offset];
}

bool m512i_arr::inc_offset(){
    offset++;
    return offset < size;
}




//Maybe the fullkey stuff should be handled by the fusion_tree code directly since the structure "belongs" to it? As in inserting a blank space for it, here we would populate it obviously
template<typename VT>
void extra_splitting(vsize_parallel_fusion_b_node<VT>* par, vsize_parallel_fusion_b_node<VT>* cur, vsize_parallel_fusion_b_node<VT>* lc, vsize_parallel_fusion_b_node<VT>* rc, int medpos, fusion_metadata old_meta) {
    // std::cout << "starting extra splitting " << medpos << std::endl;
    constexpr int old_medpos = MAX_FUSION_SIZE/2; //bit sus

    // std::cout << "half finished extra splitting" << std::endl;

    split_high_low(old_meta.fullkey, lc->fusion_internal_tree.tree.meta.fullkey, rc->fusion_internal_tree.tree.meta.fullkey, old_medpos);
    rc->fusion_internal_tree.tree.meta.fullkey>>=old_medpos+1;
    for(int i = 0; i < old_medpos; i++) {
        lc->vals[i] = cur->vals[i];
        lc->subtree_roots[i] = cur->subtree_roots[i];
    }
    for(int i = old_medpos+1; i < MAX_FUSION_SIZE; i++) {
        rc->vals[i-old_medpos-1] = cur->vals[i];
        rc->subtree_roots[i-old_medpos-1] = cur->subtree_roots[i];
    }
    // std::cout << "finished extra splitting" << std::endl;
    if(par == NULL) {
        //medpos is guaranteed to be zero when par == NULL cause there's gonna be just one element in the root
        cur->subtree_roots[0] = cur->subtree_roots[old_medpos];
        cur->vals[0] = cur->vals[old_medpos];
        cur->fusion_internal_tree.tree.meta.fullkey = (old_meta.fullkey & (1 << old_medpos)) != 0;
        // if(old_meta.fullkey & (1 << old_medpos) != 0) {
            
        //     cur->fusion_internal_tree.tree.meta.fullkey = 1;
        // }
        // else {
        //     // cur->subtree_roots[medpos] = NULL; //this seems wrong what does full key have to do with subtree_roots? That should be copied no matter what
        //     cur->fusion_internal_tree.tree.meta.fullkey = 0;
        // }
        for(int i = 1; i < MAX_FUSION_SIZE; i++) {
            cur->subtree_roots[i] = NULL;
        }
    }
    else {
        // par->vals[medpos] = cur->vals[old_medpos];
        // par->subtree_roots[medpos] = cur->subtree_roots[old_medpos];
        // par->fusion_internal_tree.tree.meta.fullkey = shift_part_left(par->fusion_internal_tree.tree.meta.fullkey, medpos);
        // par->fusion_internal_tree.tree.meta.fullkey += (old_meta.fullkey & (1 << old_medpos) != 0) << medpos;
        par->ins_val(cur->vals[old_medpos], medpos);
        par->ins_subtree(cur->subtree_roots[old_medpos], medpos);
    }
}




template<typename VT>
VariableSizeParallelFusionBTree<VT>::VariableSizeParallelFusionBTree(size_t numThreads): numThreads{numThreads}, root{numThreads}, idgen{numThreads}, locks{numThreads, 10} {
}

template<typename VT>
VariableSizeParallelFusionBTree<VT>::~VariableSizeParallelFusionBTree() {
    root.deleteSubtrees();
}

template<typename VT>
void VariableSizeParallelFusionBTree<VT>::set_val_and_new_root(VSP_BTState<VT> state, m512i_arr key, VT val, int branch, size_t thread_id) {
    //If key already there, then this just overwrites the val, I suppose?
    if (!key.inc_offset()) {
        // cout << "Inserting val " << val << " into node " << state.cur->id << endl;
        // cout << "Full key: " << state.cur->fusion_internal_tree.tree.meta.fullkey << endl;
        state.cur->set_val(val, branch);
        // cout << "Full key: " << state.cur->fusion_internal_tree.tree.meta.fullkey << endl;
        state.write_unlock_both();
        return;
    }
    else {
        if(state.cur->subtree_roots[branch] == NULL) {
            // cout << "Made new subtree root" << endl;
            state.cur->subtree_roots[branch] = new vsize_parallel_fusion_b_node<VT>(numThreads);
        }
        vsize_parallel_fusion_b_node<VT>* new_root = state.cur->subtree_roots[branch];
        state.write_unlock_both();
        return insert(new_root, key, val, thread_id);
    }
}

//keep track of how many times we "restart" in the tree?
template<typename VT>
void VariableSizeParallelFusionBTree<VT>::insert(vsize_parallel_fusion_b_node<VT>* root, m512i_arr key, VT val, size_t thread_id) {
    // cout << "Called with " << key.offset << " " << key.size << ", root id: " << root->id << endl;
    VSP_BTState<VT> state(root, numThreads, thread_id);

    __m512i* curkey = key.get_at_offset();
    // print_vec(*curkey, true);
    // print_vec(root->fusion_internal_tree.keys[0], true);

    while(true) {
        if(state.split_if_needed(extra_splitting<VT>)) {
            return insert(root, key, val, thread_id);
        }
        int branch = query_branch_node(&state.cur->fusion_internal_tree, *curkey);
        if(branch < 0) { 
            branch = ~branch;
            if(state.try_upgrade_reverse_order())
                return set_val_and_new_root(state, key, val, branch, thread_id);
            return insert(root, key, val, thread_id);
        }
        if(state.cur->children[branch] == NULL) {
            if(state.try_insert_key(*curkey, false)) {
                int branch = ~query_branch_node(&state.cur->fusion_internal_tree, *curkey);
                // print_vec(state.cur->fusion_internal_tree.keys[0], true);
                state.cur->finish_key_insert(branch);
                return set_val_and_new_root(state, key, val, branch, thread_id);
            }
            return insert(root, key, val, thread_id);
        }
        if(!state.try_HOH_readlock(state.cur->children[branch])) {
            return insert(root, key, val, thread_id);
        }
    }
}

// template<typename VT>
// void VariableSizeParallelFusionBTree<VT>::insert(m512i_arr key, VT val, size_t threadId) {
//     insert(root, key, val, thread_id);
// }

// template<typename VT>
// std::pair<VT, bool> VariableSizeParallelFusionBTree<VT>::pquery(m512i_arr key) {
//     return pquery(key, root);
// }

template<typename VT>
std::pair<VT, bool> VariableSizeParallelFusionBTree<VT>::new_root_pquery(VSP_BTState<VT> state, m512i_arr key, int branch, size_t threadId) {
    // cout << "Got here at least" << endl;
    //If key already there, then this just overwrites the val, I suppose?
    if (!key.inc_offset()) {
        // cout << "WTF2" << endl;
        if(state.cur->has_val(branch)) {
            // std::cout << "Getting t from branch " << branch << endl;
            return std::make_pair(state.cur->vals[branch], true);
        }
        else {
            // std::cout << "Getting f from branch " << branch << endl;
            return std::make_pair(state.cur->vals[branch], false);
        }
    }
    else {
        if(state.cur->subtree_roots[branch] == NULL) {
            // std::cout << "Getting f from branch " << branch << endl;
            return std::make_pair(state.cur->vals[branch], false);
        }
        vsize_parallel_fusion_b_node<VT>* new_root = state.cur->subtree_roots[branch];
        state.read_unlock_both();
        return pquery(key, new_root, threadId);
    }
}

template<typename VT>
std::pair<VT, bool> VariableSizeParallelFusionBTree<VT>::pquery(m512i_arr key, vsize_parallel_fusion_b_node<VT>* subroot, size_t threadId) {
    // cout << "Called with " << key.offset << " " << key.size << ", root id: " << subroot->id << endl;
    VSP_BTState<VT> state(subroot, numThreads, threadId); //This was root before and taking the element in the class causing problems. Like be consistent with naming or figure out another way to not have this particular problem

    __m512i* curkey = key.get_at_offset();
    // print_vec(*curkey, true);

    while(true) {
        int branch = query_branch_node(&state.cur->fusion_internal_tree, *curkey);
        if(branch < 0) { 
            branch = ~branch;
            return new_root_pquery(state, key, branch, threadId);
        }
        if(state.cur->children[branch] == NULL) { //haven't found an exact match for our portion of the key in the tree, so we know that the query is false
            // cout << "WTF" << endl;
            // print_vec(state.cur->fusion_internal_tree.keys[0], true);
            // cout << "Full key: " << state.cur->fusion_internal_tree.tree.meta.fullkey << endl;
            state.read_unlock_both();
            return std::make_pair(0, false);
        }
        if(!state.try_HOH_readlock(state.cur->children[branch])) {
            return pquery(key, subroot, threadId);
        }
    }
}

// template<typename VT>
// m512i_arr VariableSizeParallelFusionBTree<VT>::successor(m512i_arr key) {
//     return vsize_parallel_successor_DLock<VT>(root, key, thread_id);
// }

// template<typename VT>
// m512i_arr VariableSizeParallelFusionBTree<VT>::predecessor(m512i_arr key) {
//     return vsize_parallel_predecessor_DLock<VT>(root, key, thread_id);
// }

#endif
