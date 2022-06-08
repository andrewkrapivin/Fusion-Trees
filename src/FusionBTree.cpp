#include "FusionBTree.h"
#include "HelperFuncs.h"
#include <iostream>
#include <cstring>
#include <assert.h>
#include <bitset>
#include "BTreeHelper.h"

//keep track of how many times we "restart" in the tree
template<typename NodeName, bool useLock>
void parallel_insert_full_tree_DLock(NodeName* root, __m512i key, uint8_t thread_id) {
    // assert(root != NULL);

    BTState<NodeName, useLock> state(root, thread_id);

    while(true) {
        if(state.split_if_needed()) {
            return parallel_insert_full_tree_DLock<NodeName, useLock>(root, key, thread_id);
        }
        int branch = query_branch_node(&state.cur->fusion_internal_tree, key);
        if(branch < 0) { //say exact match just return & do nothing
            state.read_unlock_both();
            return;
        }
        if(state.cur->children[branch] == NULL) {
            if(state.try_insert_key(key)) {
                return;
            }
            return parallel_insert_full_tree_DLock<NodeName, useLock>(root, key, thread_id);
        }
        if(!state.try_HOH_readlock(state.cur->children[branch])) {
            return parallel_insert_full_tree_DLock<NodeName, useLock>(root, key, thread_id);
        }
    }
}

//Is this function even tested?
template<typename NodeName, bool useLock>
__m512i* parallel_successor_DLock(NodeName* root, __m512i key, uint8_t thread_id) { //returns null if there is no successor
    __m512i* retval = NULL;
    BTState<NodeName, useLock> state(root, thread_id);
    
    while(true) {
        int branch = query_branch_node(&state.cur->fusion_internal_tree, key);
        if(branch < 0) {
            branch = (~branch) + 1;
        }
        if(state.cur->fusion_internal_tree.tree.meta.size > branch) {
            retval = &state.cur->fusion_internal_tree.keys[get_real_pos_from_sorted_pos(&state.cur->fusion_internal_tree, branch)];
        }
        if(state.cur->children[branch] == NULL) {
            state.read_unlock_both();
            return retval;
        }

        if(!state.try_HOH_readlock(state.cur->children[branch])) {
            return parallel_successor_DLock<NodeName, useLock>(root, key, thread_id);
        }
    }
}

//This function is 100% not tested, kinda just copied and modified successor
template<typename NodeName, bool useLock>
__m512i* parallel_predecessor_DLock(NodeName* root, __m512i key, uint8_t thread_id) { //returns null if there is no successor
    __m512i* retval = NULL;
    BTState<NodeName, useLock> state(root, thread_id);

    while(true) {
        int branch = query_branch_node(&state.cur->fusion_internal_tree, key);
        if(branch < 0) {
            branch = (~branch);
        }
        if(branch > 0) {
            retval = &state.cur->fusion_internal_tree.keys[get_real_pos_from_sorted_pos(&state.cur->fusion_internal_tree, branch-1)];
        }
        if(state.cur->children[branch] == NULL) {
            state.read_unlock_both();
            return retval;
        }

        if(!state.try_HOH_readlock(state.cur->children[branch])) {
            return parallel_predecessor_DLock<NodeName, useLock>(root, key, thread_id);
        }
    }
}



parallel_fusion_b_node::parallel_fusion_b_node(): fusion_internal_tree(){
    for(int i=0; i<MAX_FUSION_SIZE+1; i++) {
        children[i] = NULL;
    }
    rw_lock_init(&mtx);
}

parallel_fusion_b_node::~parallel_fusion_b_node() {
    pc_destructor(&mtx.pc_counter);
}

void ParallelFusionBTree::insert(__m512i key) {
    parallel_insert_full_tree_DLock<parallel_fusion_b_node, true>(root, key, thread_id);
}

__m512i* ParallelFusionBTree::successor(__m512i key) {
    return parallel_successor_DLock<parallel_fusion_b_node, true>(root, key, thread_id);
}

__m512i* ParallelFusionBTree::predecessor(__m512i key) {
    return parallel_predecessor_DLock<parallel_fusion_b_node, true>(root, key, thread_id);
}


fusion_b_node::fusion_b_node(): fusion_internal_tree(){
    for(int i=0; i<MAX_FUSION_SIZE+1; i++) {
        children[i] = NULL;
    }
}

FusionBTree::FusionBTree() {
    root = new fusion_b_node();
}

void FusionBTree::insert(__m512i key) {
    parallel_insert_full_tree_DLock<fusion_b_node, false>(root, key, 0);
}

__m512i* FusionBTree::successor(__m512i key) {
    return parallel_successor_DLock<fusion_b_node, false>(root, key, 0);
}

__m512i* FusionBTree::predecessor(__m512i key) {
    return parallel_predecessor_DLock<fusion_b_node, false>(root, key, 0);
}