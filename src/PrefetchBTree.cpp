#include "PrefetchBTree.hpp"
#include "HelperFuncs.h"
#include <iostream>
#include <cstring>
#include <assert.h>
#include <bitset>
#include "PrefetchBTreeHelper.hpp"
#include "fusion_tree.h"

//Test out if an MLP version of just regular binary search trees can do better than this.
//Basically have say a B-tree with six or seven keys, and then an extra cache line or two for like metadata or whatever
//Prefetch the entire thing every time, and just use basic comparison to see which path to go down

using namespace std;

//keep track of how many times we "restart" in the tree
template<typename NodeName, bool useLock>
void parallel_insert_full_tree_DLock(PrefetchBTState<NodeName, useLock> state, __m512i key) {

    NodeName* root = state.cur;

    while(true) {
        if constexpr (useLock)
            state = PrefetchBTState<NodeName, useLock>{root, state.numThreads, state.threadId, state.lockTable}; //Make the semantics of this better. Not entirely sure how though, other than removing the readlocking step in the beginning. Its just that that step is pretty convenient.
        else
            state = PrefetchBTState<NodeName, useLock>{root, state.numThreads, state.threadId};
        state.start();

        while(true) {
            if(state.split_if_needed()) {
                break;
            }
            size_t branch = state.cur->query(key);
            if(branch & 1) { //say exact match just return & do nothing
                state.read_unlock_both();
                return;
            }
            branch >>= 1;
            if(state.cur->children[branch] == NULL) {
                if(state.try_insert_key(key)) {
                    return;
                }
                break;
            }
            if(!state.try_HOH_readlock(state.cur->children[branch])) {
                break;
            }
        }
    }
}

//Is this function even tested?
template<typename NodeName, bool useLock>
__m512i* parallel_successor_DLock(PrefetchBTState<NodeName, useLock> state, __m512i key) { //returns null if there is no successor

    NodeName* root = state.cur;

    while(true) {
        __m512i* retval = NULL;

        if constexpr (useLock)
            state = PrefetchBTState<NodeName, useLock>{root, state.numThreads, state.threadId, state.lockTable};
        else
            state = PrefetchBTState<NodeName, useLock>{root, state.numThreads, state.threadId};
        state.start();
        
        while(true) {
            size_t branch = (state.cur->query(key)+1);
            branch >>= 1;
            if(state.cur->curSize > branch) {
                retval = &state.cur->keys[branch];
            }
            if(state.cur->children[branch] == NULL) {
                state.read_unlock_both();
                return retval;
            }

            if(!state.try_HOH_readlock(state.cur->children[branch])) {
                break;
            }
        }
    }
}

template<typename NodeName, bool useLock>
__m512i* parallel_predecessor_DLock(PrefetchBTState<NodeName, useLock> state, __m512i key) { //returns null if there is no successor

    NodeName* root = state.cur;

    while(true) {
        __m512i* retval = NULL;

        if constexpr (useLock)
            state = PrefetchBTState<NodeName, useLock>{root, state.numThreads, state.threadId, state.lockTable};
        else
            state = PrefetchBTState<NodeName, useLock>{root, state.numThreads, state.threadId};
        state.start();
        
        while(true) {
            size_t branch = state.cur->query(key) >> 1;
            if(branch > 0) {
                retval = &state.cur->keys[branch-1];
            }
            if(state.cur->children[branch] == NULL) {
                state.read_unlock_both();
                return retval;
            }

            if(!state.try_HOH_readlock(state.cur->children[branch])) {
                break;
            }
        }
    }
}

ParallelBNode::ParallelBNode() {
    curSize = 0;
    for(size_t i{0}; i<MaxSize+1; i++) {
        children[i] = NULL;
    }
}

void ParallelBNode::deleteSubtrees() {
    for(size_t i{0}; i < curSize+1; i++) {
        if(children[i] != NULL) {
            children[i]->deleteSubtrees();
            delete children[i];
        }
    }
}

//Can be optimized a bit by not doing comparison after found location but whatever let's do this for now. But not using this function anyways
void ParallelBNode::insert(__m512i key) {
    curSize++;
    //std::cout << "Inserting key " << curSize << std::endl;
    //print_binary_uint64_big_endian(key[7], true);
    assert(curSize <= MaxSize);
    for(size_t i{0}; i < curSize; i++) {
        if (i == curSize-1 || fast_compare__m512i(key, keys[i])) { //To maintain in sorted order, we just insert. That is, if the key is smaller, swap it
            swap(key, keys[i]);
        }
    }
    //std::cout << "Result: " << std::endl;
    //for(size_t i{0}; i < curSize; i++) {
    //    print_binary_uint64_big_endian(keys[i][7], true);
    //}
}

void ParallelBNode::insert(__m512i key, size_t pos) {
    curSize++;
    assert(curSize <= MaxSize);
    for(size_t i{pos}; i < curSize; i++) {
        swap(key, keys[i]);
    }
}

bool ParallelBNode::full() {
    return curSize == MaxSize;
}

size_t ParallelBNode::query(__m512i key) {
    //std::cout << "Querying" << std::endl;
    //print_binary_uint64_big_endian(key[7], true);
    for(size_t i{0}; i < curSize; i++) {
        //print_binary_uint64_big_endian(keys[i][7], true);
        if(!fast_compare__m512i(keys[i], key)) { //if key <= keys[i], we've found either exact match or smth smaller so we done
            if(!fast_compare__m512i(key, keys[i])) { //exact match 
                //std::cout << "Result: " << (i*2+1) << endl;
                return i*2+1;
            }
            //std::cout << "Result: " << (i*2) << endl;
            return i*2;
        }
    }
    //std::cout << "Result: " << (curSize*2) << endl;
    return curSize*2;
}

//Assume split only happens when full, so just use MaxSize or curSize doesn't matter
bool ParallelBNode::split(ParallelBNode* cur, ParallelBNode* par) {
    ParallelBNode* newlefthalf = new ParallelBNode();
    ParallelBNode* newrighthalf = new ParallelBNode();

    constexpr size_t medpos = MaxSize/2;
    for(size_t i{0}; i < medpos; i++) {
        newlefthalf->keys[i] = cur->keys[i];
        newlefthalf->children[i] = cur->children[i];
    }
    newlefthalf->curSize = medpos;
    newlefthalf->children[medpos] = cur->children[medpos];
    for(size_t i{medpos+1}; i < MaxSize; i++) {
        newrighthalf->keys[i-medpos-1] = cur->keys[i];
        newrighthalf->children[i-medpos-1] = cur->children[i];
    }
    newrighthalf->curSize = MaxSize-medpos-1;
    newrighthalf->children[MaxSize-medpos-1] = cur->children[MaxSize];

    __m512i median = cur->keys[medpos];

    if(par == NULL) {
        *cur = ParallelBNode();
        cur->insert(median);
        cur->children[0] = newlefthalf;
        cur->children[1] = newrighthalf;
        return false;
    }

    size_t pos = par->query(median) >> 1;
    par->insert(median, pos);
    for(size_t i{par->curSize}; i >= pos+2; i--) {
        par->children[i] = par->children[i-1];
    }
    par->children[pos] = newlefthalf;
    par->children[pos+1] = newrighthalf;
    for(size_t i{0}; i < par->curSize+1; i++) {
        //if(par->children[i] == cur) {
        //    std::cout << "Hey you " << i << " " << pos << std::endl;
        //}
        assert(par->children[i] != cur);
    }
    return true;
}



//probably need to figure out smth better than just hardcoding the 3 locks that a fusion tree thread can hold at a time (for hand over hand locking)
ParallelBTree::ParallelBTree(size_t numThreads): numThreads{numThreads}, lockTable{numThreads}, root{} {
    for(size_t i{0}; i < numThreads; i++) {
        debugFiles.push_back(ofstream{string("debugLocks")+to_string(i)+string(".txt")});
    }
    // debugFiles.push_back(ofstream{"debugLocksWrite" + i + ".txt"});
}

void ParallelBTree::insert(__m512i key, size_t threadId) {
    PrefetchBTState<ParallelBNode, true> state(&root, numThreads, threadId, &lockTable, NULL);
    parallel_insert_full_tree_DLock(state, key);
}

__m512i* ParallelBTree::successor(__m512i key, size_t threadId) {
    PrefetchBTState<ParallelBNode, true> state(&root, numThreads, threadId, &lockTable, NULL);
    return parallel_successor_DLock(state, key);
}

__m512i* ParallelBTree::predecessor(__m512i key, size_t threadId) {
    PrefetchBTState<ParallelBNode, true> state(&root, numThreads, threadId, &lockTable, NULL);
    return parallel_predecessor_DLock(state, key);
}

ParallelBTree::~ParallelBTree() {
    root.deleteSubtrees();
}


ParallelBTreeThread::ParallelBTreeThread(ParallelBTree& tree, size_t threadId): tree(tree), threadId{threadId} {}

void ParallelBTreeThread::insert(__m512i key) {
    tree.insert(key, threadId);
}

__m512i* ParallelBTreeThread::successor(__m512i key) {
    return tree.successor(key, threadId);
}

__m512i* ParallelBTreeThread::predecessor(__m512i key) {
    return tree.predecessor(key, threadId);
}
