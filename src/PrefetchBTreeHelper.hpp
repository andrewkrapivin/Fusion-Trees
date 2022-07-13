#ifndef BTREE_HELPER_INCLUDED
#define BTREE_HELPER_INCLUDED

#include <immintrin.h>
#include <cstdint>
#include <iostream>
#include <fstream>
#include "fusion_tree.h"
#include "HashLocks.hpp"
#include "ThreadedIdGenerator.hpp"

//TODO: generalize this template for any kind of B-tree so that could do some comparative testing.
using namespace std;

template<typename NT, bool useLock = true> //NT--node type.
//NT MUST have fusion_internal_tree, children, mtx somewhere in it. I don't know how to restrict it so that that is the case, but just this is a thing. Basically, it must be related to parallel_fusion_b_node, but it can also be the variable size one
//Another thing that must be guaranteed, and this is definitely sus, is that we have the fusion node then the children arrayed next to each other in memory for clearing.
//Honestly maybe figure out a better system for this
struct PrefetchBTState {
    NT* cur;
    NT* par;
    HashMutex curMtx;
    HashMutex parMtx;
    size_t numThreads;
    size_t threadId;
    StripedLockTable* lockTable;
    uint64_t depth;
    ofstream* debug;
    PrefetchBTState(NT* root, size_t numThreads, size_t threadId);
    PrefetchBTState(NT* root, size_t numThreads, size_t threadId, StripedLockTable* lockTable, ofstream* debug = NULL, uint64_t depth = 0);
    void start(); //Read locks the root to start traversing. Maybe should call this startReadlock or smth to specify that?
    bool try_upgrade_reverse_order(); //Honestly probably better (maybe a tiny bit less efficient but w/ modern compilers who knows?) to not bother with the templating. Makes much nicer code
    void read_unlock_both();
    void write_unlock_both();
    bool split_if_needed(); //returns true if needed to split, false if did not
    // template<void (*ETS)(NT*, NT*, NT*) = [](NT* cur, NT* lc, NT* rc) -> void {}> bool split_node(); //Why this lambda function not compiling?
    // template<void (*ETS)(NT*, NT*, NT*) = [](NT* cur, NT* lc, NT* rc) -> void {}> bool split_if_needed();
    bool try_insert_key(__m512i key, bool auto_unlock = true);
    bool try_HOH_readlock(NT* child); //unlocks everything on failure
    //TODO: be consistent w/ naming & change these function names

    private:
        NT* initNode();
};




template<typename NT, bool useLock>
PrefetchBTState<NT, useLock>::PrefetchBTState(NT* root, size_t numThreads, size_t threadId): cur(root), par(NULL), numThreads(numThreads), threadId(threadId) {
    static_assert(!useLock);
}

template<typename NT, bool useLock>
PrefetchBTState<NT, useLock>::PrefetchBTState(NT* root, size_t numThreads, size_t threadId, StripedLockTable* lockTable, ofstream* debug, uint64_t depth): cur(root), par(NULL), numThreads(numThreads), threadId(threadId), lockTable{lockTable}, depth{depth}, debug{debug} {
    static_assert(useLock);
    curMtx = lockTable->getMutex((size_t) cur, depth);
}

template<typename NT, bool useLock>
void PrefetchBTState<NT, useLock>::start() {
    if constexpr (useLock) {
        curMtx.readLock(threadId);
    }
}

template<typename NT, bool useLock>
bool PrefetchBTState<NT, useLock>::try_upgrade_reverse_order() {
    if constexpr (useLock) {
        if(!curMtx.tryPartialUpgrade(threadId)) {
            if(par != NULL) {
                parMtx.readUnlock(threadId);
            }
            return false;
        }
        if (par == NULL) {
            curMtx.finishPartialUpgrade();
            return true;
        }
        if(!parMtx.tryPartialUpgrade(threadId)) {
            curMtx.partialUpgradeUnlock();
            return false;
        }
        parMtx.finishPartialUpgrade();
        curMtx.finishPartialUpgrade();
        return true;
    }
    return true;
}

template<typename NT, bool useLock>
void PrefetchBTState<NT, useLock>::read_unlock_both() {
    if constexpr (useLock) {
        curMtx.readUnlock(threadId);
        if(par != NULL) parMtx.readUnlock(threadId);
    }
}

template<typename NT, bool useLock>
void PrefetchBTState<NT, useLock>::write_unlock_both() {
    if constexpr (useLock) {
        curMtx.writeUnlock();
        if(par != NULL) parMtx.writeUnlock();
    }
}

//Function probably no longer needed. Remove?
template<typename NT, bool useLock>
NT* PrefetchBTState<NT, useLock>::initNode() {
    return new NT();
}

template<typename NT, bool useLock>
bool PrefetchBTState<NT, useLock>::split_if_needed() {
    if(cur->full()) {
        if(!try_upgrade_reverse_order()) { //Somewhat misleading but basically to tell you that you need to restart the inserting process
            return true;
        }
        if(!NT::split(cur, par)) { //Return statement is just equal to par != NULL so maybe don't bother with this return statement? Can simplfiy ex next couple lines?
            if constexpr (useLock) 
                curMtx.writeUnlock();
        }
        else {
            if constexpr (useLock) 
                curMtx.writeUnlock();
            delete cur;
        }
        if constexpr (useLock) {
            if(par != NULL)
                parMtx.writeUnlock();
        }
        return true;
    }
    return false; //Tells you split was unnecessary
}

template<typename NT, bool useLock>
bool PrefetchBTState<NT, useLock>::try_insert_key(__m512i key, bool auto_unlock) {
    if(try_upgrade_reverse_order()) {
        cur->insert(key);
        if (auto_unlock)
            write_unlock_both();
        return true;
    }
    return false;
}

template<typename NT, bool useLock>
bool PrefetchBTState<NT, useLock>::try_HOH_readlock(NT* child) {
    if constexpr (useLock) {
        depth++;
        HashMutex childMtx = lockTable->getMutex((size_t) child, depth);
        if(!childMtx.tryReadLock(threadId)) {
            read_unlock_both();
            return false;
        }
        if(par != NULL)
            parMtx.readUnlock(threadId);
        parMtx = curMtx;
        curMtx = childMtx;
    }
    par = cur;
    cur = child;
    return true;
}

#endif
