#include "FusionBTree.h"
#include "HelperFuncs.h"
#include <iostream>
#include <cstring>
#include <assert.h>
#include <bitset>
#include "BTreeHelper.hpp"

atomic<uint64_t> numBad{0};
atomic<uint64_t> numBadWithMultiplicity{0};

//keep track of how many times we "restart" in the tree
template<typename NodeName, bool useLock>
void parallel_insert_full_tree_DLock(BTState<NodeName, useLock> state, __m512i key) {
    // assert(root != NULL);

    // BTState<NodeName, useLock, useHashLock> stateCpy = state;
    NodeName* root = state.cur;
    // uint64_t count = 0;
    // bool bad = false;
    while(true) {
        // state = stateCpy;
        if constexpr (useLock)
            state = BTState<NodeName, useLock>{root, state.numThreads, state.threadId, state.lockTable}; //Make the semantics of this better. Not entirely sure how though, other than removing the readlocking step in the beginning. Its just that that step is pretty convenient.
        else
            state = BTState<NodeName, useLock>{root, state.numThreads, state.threadId};
        state.start();
         //not great should fix later

        while(true) {
            if(state.split_if_needed()) {
                // std::cout << "NANDE " << std::endl;
                // return parallel_insert_full_tree_DLock<NodeName, useLock, useHashLock>(BTState<NodeName, useLock, useHashLock>{root, state.numThreads, state.threadId, state.lockTable, state.idGen}, key);
                // std::cout << "NANDE2" << std::endl;
                break;
            }
            int branch = query_branch_node(&state.cur->fusion_internal_tree, key);
            if(branch < 0) { //say exact match just return & do nothing
                state.read_unlock_both();
                return;
            }
            if(state.cur->children[branch] == NULL) {
                if(state.try_insert_key(key)) {
                    // if(bad) {
                    //     cout << (numBad++) << " " << numBadWithMultiplicity << endl;
                    // }
                    return;
                }
                // std::cout << "NANDE3" << std::endl;
                // return parallel_insert_full_tree_DLock<NodeName, useLock, useHashLock>(BTState<NodeName, useLock, useHashLock>{root, state.numThreads, state.threadId, state.lockTable, state.idGen}, key);
                break;
            }
            if(!state.try_HOH_readlock(state.cur->children[branch])) {
                // std::cout << "NANDE4" << std::endl;
                // return parallel_insert_full_tree_DLock<NodeName, useLock, useHashLock>(BTState<NodeName, useLock, useHashLock>{root, state.numThreads, state.threadId, state.lockTable, state.idGen}, key);
                break;
            }
        }
        // count++;
        // if(count > 1000) {
        //     bad = true;
        //     numBadWithMultiplicity++;
        // }
    }
}

//Is this function even tested?
template<typename NodeName, bool useLock>
__m512i* parallel_successor_DLock(BTState<NodeName, useLock> state, __m512i key) { //returns null if there is no successor

    NodeName* root = state.cur;

    while(true) {
        __m512i* retval = NULL;

        if constexpr (useLock)
            state = BTState<NodeName, useLock>{root, state.numThreads, state.threadId, state.lockTable};
        else
            state = BTState<NodeName, useLock>{root, state.numThreads, state.threadId};
        state.start();
        
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
                // return parallel_successor_DLock<NodeName, useLock, useHashLock>(BTState<NodeName, useLock, useHashLock>{root, state.numThreads, state.threadId, state.lockTable, state.idGen}, key);
                break;
            }
        }
    }
}

template<typename NodeName, bool useLock>
__m512i* parallel_predecessor_DLock(BTState<NodeName, useLock> state, __m512i key) { //returns null if there is no successor

    NodeName* root = state.cur;

    while(true) {
        __m512i* retval = NULL;

        if constexpr (useLock)
            state = BTState<NodeName, useLock>{root, state.numThreads, state.threadId, state.lockTable};
        else
            state = BTState<NodeName, useLock>{root, state.numThreads, state.threadId};
        state.start();
        
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
                // return parallel_predecessor_DLock<NodeName, useLock, useHashLock>(BTState<NodeName, useLock, useHashLock>{root, state.numThreads, state.threadId, state.lockTable, state.idGen}, key);
                break;
            }
        }
    }
}



// ParallelFusionBNode::ParallelFusionBNode(LockHashTable* table, size_t id): fusion_internal_tree(), mtx{table, id} {
//     for(int i=0; i<MAX_FUSION_SIZE+1; i++) {
//         children[i] = NULL;
//     }
//     // rw_lock_init(&mtx);
// }
ParallelFusionBNode::ParallelFusionBNode(): fusion_internal_tree() {
    for(int i=0; i<MAX_FUSION_SIZE+1; i++) {
        children[i] = NULL;
    }
    // rw_lock_init(&mtx);
}

// parallel_fusion_b_node::~parallel_fusion_b_node() {
//     // pc_destructor(&mtx.pc_counter);
// }

//probably need to figure out smth better than just hardcoding the 3 locks that a fusion tree thread can hold at a time (for hand over hand locking)
ParallelFusionBTree::ParallelFusionBTree(size_t numThreads): numThreads{numThreads}, lockTable{numThreads}, root{} {
    for(size_t i{0}; i < numThreads; i++) {
        debugFiles.push_back(ofstream{string("debugLocks")+to_string(i)+string(".txt")});
    }
    // debugFiles.push_back(ofstream{"debugLocksWrite" + i + ".txt"});
}

void ParallelFusionBTree::insert(__m512i key, size_t threadId) {
    BTState<ParallelFusionBNode, true> state(&root, numThreads, threadId, &lockTable, NULL);
    parallel_insert_full_tree_DLock(state, key);
}

__m512i* ParallelFusionBTree::successor(__m512i key, size_t threadId) {
    BTState<ParallelFusionBNode, true> state(&root, numThreads, threadId, &lockTable, NULL);
    return parallel_successor_DLock(state, key);
}

__m512i* ParallelFusionBTree::predecessor(__m512i key, size_t threadId) {
    BTState<ParallelFusionBNode, true> state(&root, numThreads, threadId, &lockTable, NULL);
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
    parallel_insert_full_tree_DLock(state, key);
}

__m512i* FusionBTree::successor(__m512i key) {
    BTState<fusion_b_node, false> state(root, 0, 0);
    return parallel_successor_DLock(state, key);
}

__m512i* FusionBTree::predecessor(__m512i key) {
    BTState<fusion_b_node, false> state(root, 0, 0);
    return parallel_predecessor_DLock(state, key);
}