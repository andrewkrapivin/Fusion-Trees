#include "VariableSizeFusionBTree.h"
#include "HelperFuncs.h"
#include <iostream>
#include <cstring>
#include <assert.h>
#include <bitset>

template<typename VT>
vsize_parallel_fusion_b_node<VT>::vsize_parallel_fusion_b_node(): fusion_internal_tree(){
    for(int i=0; i<MAX_FUSION_SIZE+1; i++) {
        children[i] = NULL;
    }
    for(int i=0; i<MAX_FUSION_SIZE; i++) {
        vals[i] = NULL;
    }
    for(int i=0; i<MAX_FUSION_SIZE; i++) {
        subtree_roots[i] = NULL;
    }
    rw_lock_init(&mtx);
}

template<typename VT>
vsize_parallel_fusion_b_node<VT>::~vsize_parallel_fusion_b_node() {
    pc_destructor(&mtx.pc_counter);
}

template<typename VT>
void vsize_parallel_fusion_b_node<VT>::set_val(VT val, int pos) {
    vals[pos] = val;
    fusion_internal_tree.tree.meta.fullkey |= 1 << pos;
}

template<typename VT>
bool vsize_parallel_fusion_b_node<VT>::has_val(int pos) {
    return fusion_internal_tree.tree.meta.fullkey & (1 << pos);
}


//Sus constructor cause not destructed that way? idk
m512i_arr::m512i_arr() {
    m512i_arr(default_size);
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

//Maybe the fullkey stuff should be handled by the fusion_tree code directly since the structure "belongs" to it? As in inserting a blank space for it, here we would populate it obviously
template<typename VT>
void extra_splitting(vsize_parallel_fusion_b_node<VT>* par, vsize_parallel_fusion_b_node<VT>* cur, vsize_parallel_fusion_b_node<VT>* lc, vsize_parallel_fusion_b_node<VT>* rc, int medpos, fusion_metadata old_meta) {
    constexpr int old_medpos = MAX_FUSION_SIZE/2; //bit sus
    
    if(par == NULL) {
        if(old_meta.fullkey & (1 << old_medpos) != 0) {
            par->vals[medpos] = cur->vals[old_medpos];
            par->subtree_roots[medpos] = cur->subtree_roots[old_medpos];
            par->fusion_internal_tree.tree.meta.fullkey = 1;
        }
        else {
            par->subtree_roots[medpos] = NULL;
            par->fusion_internal_tree.tree.meta.fullkey = 0;
        }
        for(int i = 1; i < MAX_FUSION_SIZE; i++) {
            par->subtree_roots[i] = NULL;
        }
    }
    else {
        par->vals[medpos] = cur->vals[old_medpos];
        par->subtree_roots[medpos] = cur->subtree_roots[old_medpos];
        par->fusion_internal_tree.tree.meta.fullkey = shift_part_left(par->fusion_internal_tree.tree.meta.fullkey, medpos);
        par->fusion_internal_tree.tree.meta.fullkey += (old_meta.fullkey & (1 << old_medpos) != 0) << medpos;
    }

    split_high_low(old_meta.fullkey, lc->fusion_internal_tree.tree.meta.fullkey, rc->fusion_internal_tree.tree.meta.fullkey, medpos);
    rc->fusion_internal_tree.tree.meta.fullkey>>=medpos+1;
    for(int i = 0; i < medpos; i++) {
        lc->vals[i] = cur->vals[i];
        lc->subtree_roots[i] = cur->subtree_roots[i];
    }
    for(int i = medpos+1; i < MAX_FUSION_SIZE; i++) {
        rc->vals[i-medpos-1] = cur->vals[i];
        rc->subtree_roots[i-medpos-1] = cur->subtree_roots[i];
    }
}

template<typename VT>
void vsize_parallel_insert_full_tree_DLock(vsize_parallel_fusion_b_node<VT>* root, m512i_arr key, VT val, uint8_t thread_id);

template<typename VT>
void set_val_and_new_root(VSP_BTState<VT> state, m512i_arr key, VT val, int branch, uint8_t thread_id) {
    //If key already there, then this just overwrites the val, I suppose?
    if (!key.inc_offset()) {
        state.cur->set_val(val, branch);
    }
    else {
        if(state.cur->subtree_roots[branch] == NULL) {
            state.cur->subtree_roots[branch] = new vsize_parallel_fusion_b_node<VT>();
        }
        vsize_parallel_fusion_b_node<VT>* new_root = state.cur->subtree_roots[branch];
        state.read_unlock_both();
        return vsize_parallel_insert_full_tree_DLock(new_root, key, val, thread_id);
    }
}

//keep track of how many times we "restart" in the tree?
template<typename VT>
void vsize_parallel_insert_full_tree_DLock(vsize_parallel_fusion_b_node<VT>* root, m512i_arr key, VT val, uint8_t thread_id) {
    VSP_BTState<VT> state(root, thread_id);

    __m512i* curkey = key.get_at_offset();

    while(true) {
        if(state.split_if_needed(extra_splitting<VT>)) {
            return vsize_parallel_insert_full_tree_DLock(root, key, val, thread_id);
        }
        int branch = query_branch_node(&state.cur->fusion_internal_tree, *curkey);
        if(branch < 0) { 
            branch = ~branch;
            return set_val_and_new_root<VT>(state, key, val, branch, thread_id);
        }
        if(state.cur->children[branch] == NULL) {
            if(state.try_insert_key(*curkey)) {
                int branch = ~query_branch_node(&state.cur->fusion_internal_tree, *curkey);
                return set_val_and_new_root<VT>(state, key, val, branch, thread_id);
            }
            return vsize_parallel_insert_full_tree_DLock(root, key, val, thread_id);
        }
        if(!state.try_HOH_readlock(state.cur->children[branch])) {
            return vsize_parallel_insert_full_tree_DLock(root, key, val, thread_id);
        }
    }
}


//temporary function need to redo the return val here
template<typename VT>
m512i_arr vsize_parallel_successor_DLock(vsize_parallel_fusion_b_node<VT>* root, m512i_arr key, uint8_t thread_id) { //returns null if there is no successor
    return m512i_arr(NULL, 0);
    
    // __m512i* retval = NULL;
    // VSP_BTState<VT> state(root, thread_id);
    
    // while(true) {
    //     int branch = query_branch_node(&state.cur->fusion_internal_tree, key);
    //     if(branch < 0) {
    //         branch = (~branch) + 1;
    //     }
    //     if(state.cur->fusion_internal_tree.tree.meta.size > branch) {
    //         retval = &state.cur->fusion_internal_tree.keys[get_real_pos_from_sorted_pos(&state.cur->fusion_internal_tree, branch)];
    //     }
    //     if(state.cur->children[branch] == NULL) {
    //         state.read_unlock_both();
    //         return retval;
    //     }

    //     if(!state.try_HOH_readlock(state.cur->children[branch])) {
    //         return parallel_successor_DLock(root, key, thread_id);
    //     }
    // }
}

template<typename VT>
m512i_arr vsize_parallel_predecessor_DLock(vsize_parallel_fusion_b_node<VT>* root, m512i_arr key, uint8_t thread_id) { //returns null if there is no successor
    return m512i_arr(NULL, 0);

    // __m512i* retval = NULL;
    // VSP_BTState<VT> state(root, thread_id);

    // while(true) {
    //     int branch = query_branch_node(&state.cur->fusion_internal_tree, key);
    //     if(branch < 0) {
    //         branch = (~branch);
    //     }
    //     if(branch > 0) {
    //         retval = &state.cur->fusion_internal_tree.keys[get_real_pos_from_sorted_pos(&state.cur->fusion_internal_tree, branch-1)];
    //     }
    //     if(state.cur->children[branch] == NULL) {
    //         state.read_unlock_both();
    //         return retval;
    //     }

    //     if(!state.try_HOH_readlock(state.cur->children[branch])) {
    //         return parallel_predecessor_DLock(root, key, thread_id);
    //     }
    // }
}


template<typename VT>
void VariableSizeParallelFusionBTree<VT>::insert(m512i_arr key, VT val) {
    vsize_parallel_insert_full_tree_DLock<VT>(root, key, val, thread_id);
}

template<typename VT>
std::pair<VT, bool> VariableSizeParallelFusionBTree<VT>::pquery(m512i_arr key) {
    return pquery(key, root);
}

template<typename VT>
std::pair<VT, bool> VariableSizeParallelFusionBTree<VT>::new_root_pquery(VSP_BTState<VT> state, m512i_arr key, int branch) {
    //If key already there, then this just overwrites the val, I suppose?
    if (!key.inc_offset()) {
        if(state.cur.has_val(branch)) {
            return std::make_pair(state.cur.vals[branch], true);
        }
        else {
            return std::make_pair(state.cur.vals[branch], false);
        }
    }
    else {
        if(state.cur->subtree_roots[branch] == NULL) {
            return std::make_pair(state.cur.vals[branch], false);
        }
        vsize_parallel_fusion_b_node<VT>* new_root = state.cur->subtree_roots[branch];
        state.read_unlock_both();
        return pquery(new_root, new_root);
    }
}

template<typename VT>
std::pair<VT, bool> VariableSizeParallelFusionBTree<VT>::pquery(m512i_arr key, vsize_parallel_fusion_b_node<VT>* subroot) {
    VSP_BTState<VT> state(root, thread_id);

    __m512i* curkey = key.get_at_offset();

    while(true) {
        if(state.split_if_needed(extra_splitting<VT>)) {
            return pquery(key, subroot);
        }
        int branch = query_branch_node(&state.cur->fusion_internal_tree, *curkey);
        if(branch < 0) { 
            branch = ~branch;
            return new_root_pquery(state, key, branch);
        }
        if(state.cur->children[branch] == NULL) {
            if(state.try_insert_key(*curkey)) {
                int branch = ~query_branch_node(&state.cur->fusion_internal_tree, *curkey);
                return new_root_pquery(state, key, branch);
            }
            return pquery(key, subroot);
        }
        if(!state.try_HOH_readlock(state.cur->children[branch])) {
            return pquery(key, subroot);
        }
    }
}

template<typename VT>
m512i_arr VariableSizeParallelFusionBTree<VT>::successor(m512i_arr key) {
    return vsize_parallel_successor_DLock<VT>(root, key, thread_id);
}

template<typename VT>
m512i_arr VariableSizeParallelFusionBTree<VT>::predecessor(m512i_arr key) {
    return vsize_parallel_predecessor_DLock<VT>(root, key, thread_id);
}