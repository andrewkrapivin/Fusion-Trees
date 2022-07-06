#include "FusionBTree.h"
#include "HelperFuncs.h"
#include <iostream>
#include <cstring>
#include <assert.h>
#include <bitset>
#include "BTreeHelper.hpp"

//keep track of how many times we "restart" in the tree
template<typename NodeName, bool useLock, bool useHashLock>
void parallel_insert_full_tree_DLock(BTState<NodeName, useLock, useHashLock> state, __m512i key) {
    // assert(root != NULL);

    NodeName* root = state.cur; //not great should fix later

    while(true) {
        if(state.split_if_needed()) {
            // std::cout << "NANDE " << std::endl;
            return parallel_insert_full_tree_DLock<NodeName, useLock, useHashLock>(BTState<NodeName, useLock, useHashLock>{root, state.numThreads, state.threadId, state.lockTable, state.idGen}, key);
            // std::cout << "NANDE2" << std::endl;
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
            // std::cout << "NANDE3" << std::endl;
            return parallel_insert_full_tree_DLock<NodeName, useLock, useHashLock>(BTState<NodeName, useLock, useHashLock>{root, state.numThreads, state.threadId, state.lockTable, state.idGen}, key);
        }
        if(!state.try_HOH_readlock(state.cur->children[branch])) {
            // std::cout << "NANDE4" << std::endl;
            return parallel_insert_full_tree_DLock<NodeName, useLock, useHashLock>(BTState<NodeName, useLock, useHashLock>{root, state.numThreads, state.threadId, state.lockTable, state.idGen}, key);
        }
    }
}

//Is this function even tested?
template<typename NodeName, bool useLock, bool useHashLock>
__m512i* parallel_successor_DLock(BTState<NodeName, useLock, useHashLock> state, __m512i key) { //returns null if there is no successor
    __m512i* retval = NULL;

    NodeName* root = state.cur;
    
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
            return parallel_successor_DLock<NodeName, useLock, useHashLock>(BTState<NodeName, useLock, useHashLock>{root, state.numThreads, state.threadId, state.lockTable, state.idGen}, key);
        }
    }
}

template<typename NodeName, bool useLock, bool useHashLock>
__m512i* parallel_predecessor_DLock(BTState<NodeName, useLock, useHashLock> state, __m512i key) { //returns null if there is no successor
    __m512i* retval = NULL;

    NodeName* root = state.cur;
    
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
            return parallel_predecessor_DLock<NodeName, useLock, useHashLock>(BTState<NodeName, useLock, useHashLock>{root, state.numThreads, state.threadId, state.lockTable, state.idGen}, key);
        }
    }
}



ParallelFusionBNode::ParallelFusionBNode(size_t numThreads): fusion_internal_tree(), mtx{numThreads} {
    for(int i=0; i<MAX_FUSION_SIZE+1; i++) {
        children[i] = NULL;
    }
    // rw_lock_init(&mtx);
}

// parallel_fusion_b_node::~parallel_fusion_b_node() {
//     // pc_destructor(&mtx.pc_counter);
// }

//probably need to figure out smth better than just hardcoding the 3 locks that a fusion tree thread can hold at a time (for hand over hand locking)
ParallelFusionBTree::ParallelFusionBTree(size_t numThreads): numThreads{numThreads}, lockTable{numThreads, 3}, idGen{numThreads}, root{numThreads} {}

void ParallelFusionBTree::insert(__m512i key, size_t threadId) {
    BTState<ParallelFusionBNode, true, false> state(&root, numThreads, threadId, &lockTable, &idGen);
    parallel_insert_full_tree_DLock(state, key);
}

__m512i* ParallelFusionBTree::successor(__m512i key, size_t threadId) {
    BTState<ParallelFusionBNode, true, false> state(&root, numThreads, threadId, &lockTable, &idGen);
    return parallel_successor_DLock(state, key);
}

__m512i* ParallelFusionBTree::predecessor(__m512i key, size_t threadId) {
    BTState<ParallelFusionBNode, true, false> state(&root, numThreads, threadId, &lockTable, &idGen);
    return parallel_predecessor_DLock(state, key);
}


ParallelFusionBTreeThread::ParallelFusionBTreeThread(ParallelFusionBTree& tree, size_t threadId): tree(tree), threadId{threadId} {}

void ParallelFusionBTreeThread::insert(__m512i key) {
    tree.insert(key, threadId);
}

__m512i* ParallelFusionBTreeThread::successor(__m512i key) {
    return tree.successor(key, threadId);
}

__m512i* ParallelFusionBTreeThread::predecessor(__m512i key) {
    return tree.predecessor(key, threadId);
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
    BTState<fusion_b_node, false> state(root, 0, 0);
    parallel_insert_full_tree_DLock<fusion_b_node, false, false>(state, key);
}

__m512i* FusionBTree::successor(__m512i key) {
    BTState<fusion_b_node, false> state(root, 0, 0);
    return parallel_successor_DLock<fusion_b_node, false, false>(state, key);
}

__m512i* FusionBTree::predecessor(__m512i key) {
    BTState<fusion_b_node, false> state(root, 0, 0);
    return parallel_predecessor_DLock<fusion_b_node, false, false>(state, key);
}