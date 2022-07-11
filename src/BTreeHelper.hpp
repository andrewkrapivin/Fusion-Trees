#ifndef BTREE_HELPER_INCLUDED
#define BTREE_HELPER_INCLUDED

#include <immintrin.h>
#include <cstdint>
#include <iostream>
#include <fstream>
#include "lock.h"
#include "fusion_tree.h"
#include "HashLocks.hpp"
#include "ThreadedIdGenerator.hpp"

//TODO: generalize this template for any kind of B-tree so that could do some comparative testing.
using namespace std;

template<typename NT, bool useLock = true, bool useHashLock = false> //NT--node type.
//NT MUST have fusion_internal_tree, children, mtx somewhere in it. I don't know how to restrict it so that that is the case, but just this is a thing. Basically, it must be related to parallel_fusion_b_node, but it can also be the variable size one
//Another thing that must be guaranteed, and this is definitely sus, is that we have the fusion node then the children arrayed next to each other in memory for clearing.
//Honestly maybe figure out a better system for this
struct BTState {
    // template<bool HP>
    // static void emptyfunc(NT* cur, NT* lc, NT* rc) {
    // }
    static void emptyfunc(NT* par, NT* cur, NT* lc, NT* rc, int medpos, fusion_metadata old_meta) {
        (void)par;
        (void)cur;
        (void)lc;
        (void)rc;
        (void)medpos;
        (void)old_meta;
    }

    NT* cur;
    NT* par;
    HashMutex curMtx;
    HashMutex parMtx;
    size_t numThreads;
    size_t threadId;
    // LockHashTable* lockTable;
    StripedLockTable* lockTable;
    uint64_t depth;
    ThreadedIdGenerator* idGen;
    ofstream* debug;
    // template<bool HP> NT (*extra_splitting)(NT*, NT*, NT*);
    BTState(NT* root, size_t numThreads, size_t threadId);
    BTState(NT* root, size_t numThreads, size_t threadId, StripedLockTable* lockTable, ThreadedIdGenerator* idGen, ofstream* debug = NULL, uint64_t depth = 0);
    // template<bool HP> bool try_upgrade_reverse_order(); //HP: has par
    // template<bool HP> void read_unlock_both();
    // template<bool HP> void write_unlock_both();
    // template<bool HP, void (*ETS)(NT*, NT*, NT*) = emptyfunc> bool split_node(); //splits node and in the middle does extra splitting
    // template<bool HP, void (*ETS)(NT*, NT*, NT*) = emptyfunc> bool split_if_needed(); //returns true if needed to split, false if did not
    // template<bool HP> void try_insert_key(__m512i key);
    // template<bool HP> bool try_HOH_readlock(NT* child); //unlocks everything on failure
    bool try_upgrade_reverse_order(); //Honestly probably better (maybe a tiny bit less efficient but w/ modern compilers who knows?) to not bother with the templating. Makes much nicer code
    void read_unlock_both();
    void write_unlock_both();
    // template<> 
    bool split_node(void ETS(NT*, NT*, NT*, NT*, int, fusion_metadata) = emptyfunc); //splits node and in the middle does extra splitting
    // template<void ETS(NT*, NT*, NT*, NT*, int, fusion_metadata) = emptyfunc> 
    bool split_if_needed(void ETS(NT*, NT*, NT*, NT*, int, fusion_metadata) = emptyfunc); //returns true if needed to split, false if did not
    // template<void (*ETS)(NT*, NT*, NT*) = [](NT* cur, NT* lc, NT* rc) -> void {}> bool split_node(); //Why this lambda function not compiling?
    // template<void (*ETS)(NT*, NT*, NT*) = [](NT* cur, NT* lc, NT* rc) -> void {}> bool split_if_needed();
    bool try_insert_key(__m512i key, bool auto_unlock = true);
    bool try_HOH_readlock(NT* child); //unlocks everything on failure
    //TODO: be consistent w/ naming & change these function names

    private:
        NT* initNode();
};




template<typename NT, bool useLock, bool useHashLock>
BTState<NT, useLock, useHashLock>::BTState(NT* root, size_t numThreads, size_t threadId): cur(root), par(NULL), numThreads(numThreads), threadId(threadId) {
    // if constexpr (useLock)
    //     read_lock(&curMtx, WAIT_FOR_LOCK, thread_id);
    if constexpr (useLock)
        curMtx.readLock(threadId);
}

template<typename NT, bool useLock, bool useHashLock>
BTState<NT, useLock, useHashLock>::BTState(NT* root, size_t numThreads, size_t threadId, StripedLockTable* lockTable, ThreadedIdGenerator* idGen, ofstream* debug, uint64_t depth): cur(root), par(NULL), numThreads(numThreads), threadId(threadId), lockTable{lockTable}, depth{depth}, idGen{idGen}, debug{debug} {
    // if constexpr (useLock)
    //     read_lock(&curMtx, WAIT_FOR_LOCK, thread_id);
    if constexpr (useLock) {
        // curMtx = HashMutex(lockTable, (size_t) cur);
        curMtx = lockTable->getMutex((size_t) cur, depth);
        if(debug) {
            if constexpr (useHashLock)
                *debug << "Read locking " << curMtx.getId() << endl;
        }
        curMtx.readLock(threadId);
        if(debug) {
            if constexpr (useHashLock)
                *debug << "Finished read lock" << endl;
        }
    }
}

template<typename NT, bool useLock, bool useHashLock>
// template<bool HP>
bool BTState<NT, useLock, useHashLock>::try_upgrade_reverse_order() {
    if constexpr (useLock) {
        // if(!partial_upgrade(&curMtx, TRY_ONCE_LOCK, thread_id)) {
        //     // if(HP)
        //     if(par != NULL)
        //         read_unlock(&parMtx, thread_id);
        //     return false;
        // }
        // if (par == NULL) {
        //     finish_partial_upgrade(&curMtx);
        //     return true;
        // }
        // if(!partial_upgrade(&parMtx, TRY_ONCE_LOCK, thread_id)) {
        //     unlock_partial_upgrade(&curMtx);
        //     return false;
        // }
        // finish_partial_upgrade(&parMtx);
        // finish_partial_upgrade(&curMtx);
        // return true;
        if(debug) {
            if constexpr (useHashLock)
                *debug << "Trying partial upgrade of " << curMtx.getId() << endl;
        }
        if(!curMtx.tryPartialUpgrade(threadId)) {
            if(debug) {
                if constexpr (useHashLock)
                    *debug << "Failed" << endl;
            }
            if(par != NULL) {
                if(debug) {
                    if constexpr (useHashLock)
                        *debug << "Read unlocking " << parMtx.getId() << endl;
                }
                parMtx.readUnlock(threadId);
                if(debug) {
                    if constexpr (useHashLock)
                        *debug << "Finished" << endl;
                }
            }
            return false;
        }
        if(debug) {
            if constexpr (useHashLock)
                *debug << "Succeeded" << endl;
        }
        if (par == NULL) {
            if(debug) {
                if constexpr (useHashLock)
                    *debug << "Finishing partial upgrade of " << curMtx.getId() << endl;
            }
            curMtx.finishPartialUpgrade();
            if(debug) {
                if constexpr (useHashLock)
                    *debug << "Finished" << endl;
            }
            return true;
        }
        if(debug) {
            if constexpr (useHashLock)
                *debug << "Trying partial upgrade of " << parMtx.getId() << endl;
        }
        if(!parMtx.tryPartialUpgrade(threadId)) {
            if(debug) {
                if constexpr (useHashLock)
                    *debug << "Failed" << endl;
            }
            curMtx.partialUpgradeUnlock();
            if(debug) {
                if constexpr (useHashLock)
                    (*debug) << "Finished partial upgrade unlock of " << curMtx.getId() << endl;
            }
            return false;
        }
        parMtx.finishPartialUpgrade();
        if(debug) {
            if constexpr (useHashLock)
                (*debug) << "Finished partial upgrade unlock of " << parMtx.getId() << endl;
        }
        curMtx.finishPartialUpgrade();
        if(debug) {
            if constexpr (useHashLock)
                (*debug) << "Finished partial upgrade unlock of " << curMtx.getId() << endl;
        }
        return true;
    }
    return true;
}

template<typename NT, bool useLock, bool useHashLock>
// template<bool HP>
void BTState<NT, useLock, useHashLock>::read_unlock_both() {
    if constexpr (useLock) {
        // read_unlock(&curMtx, thread_id);
        // if(par != NULL) read_unlock(&parMtx, thread_id);
        curMtx.readUnlock(threadId);
        if(par != NULL) parMtx.readUnlock(threadId);
    }
}

template<typename NT, bool useLock, bool useHashLock>
// template<bool HP>
void BTState<NT, useLock, useHashLock>::write_unlock_both() {
    if constexpr (useLock) {
        // write_unlock(&curMtx);
        curMtx.writeUnlock();
        // if(par != NULL) write_unlock(&parMtx);
        if(par != NULL) parMtx.writeUnlock();
    }
}

template<typename NT, bool useLock, bool useHashLock>
NT* BTState<NT, useLock, useHashLock>::initNode() {
    if constexpr (useLock) {
        if constexpr (useHashLock) {
            // return new NT{lockTable, (*idGen)(threadId)};
            return new NT();
        }
        else {
            return new NT{numThreads};
        }
    }
    else {
        return new NT();
    }
}

template<typename NT, bool useLock, bool useHashLock>
// template<void ETS(NT*, NT*, NT*, NT*, int, fusion_metadata)> //Why does this templated version not work??? It should...
bool BTState<NT, useLock, useHashLock>::split_node(void ETS(NT*, NT*, NT*, NT*, int, fusion_metadata)) {
    // fusion_node* key_fnode = &cur->fusion_internal_tree;

    NT* newlefthalf = initNode();
    NT* newrighthalf = initNode();
    // if constexpr (useLock) {
    //     newlefthalf = new NT(numThreads);
    //     newrighthalf = new NT(numThreads);
    // }
    // else {
    //     newlefthalf = new NT();
    //     newrighthalf = new NT();
    // }

    constexpr int medpos = MAX_FUSION_SIZE/2; //two choices since max size even: maybe randomize?
    for(int i = 0; i < medpos; i++) {
        insert(&newlefthalf->fusion_internal_tree, get_key_from_sorted_pos(&cur->fusion_internal_tree, i));
        newlefthalf->children[i] = cur->children[i];
    }
    newlefthalf->children[medpos] = cur->children[medpos];
    for(int i = medpos+1; i < MAX_FUSION_SIZE; i++) {
        insert(&newrighthalf->fusion_internal_tree, get_key_from_sorted_pos(&cur->fusion_internal_tree, i));
        newrighthalf->children[i-medpos-1] = cur->children[i];
    }
    newrighthalf->children[MAX_FUSION_SIZE-medpos-1] = cur->children[MAX_FUSION_SIZE];

    if(cur->fusion_internal_tree.tree.meta.fast) {
        make_fast(&newlefthalf->fusion_internal_tree);
        make_fast(&newrighthalf->fusion_internal_tree);
    }

    __m512i median = get_key_from_sorted_pos(&cur->fusion_internal_tree, medpos);
    fusion_metadata old_meta = cur->fusion_internal_tree.tree.meta;

    if(par == NULL) {
        memset(&cur->fusion_internal_tree, 0, sizeof(fusion_node) + sizeof(NT*)*(MAX_FUSION_SIZE+1));
        insert_key_node(&cur->fusion_internal_tree, median);
        make_fast(&cur->fusion_internal_tree);
        cur->children[0] = newlefthalf;
        cur->children[1] = newrighthalf;
        ETS(par, cur, newlefthalf, newrighthalf, 0, old_meta);
        return false;
    }

    insert_key_node(&par->fusion_internal_tree, median);
    int pos = ~query_branch_node(&par->fusion_internal_tree, median);
    for(int i=par->fusion_internal_tree.tree.meta.size /*ok that is ridiculous, fix that*/; i >= pos+2; i--) {
        par->children[i] = par->children[i-1];
    }
    par->children[pos] = newlefthalf;
    par->children[pos+1] = newrighthalf;
    ETS(par, cur, newlefthalf, newrighthalf, pos, old_meta);
    return true;
}

template<typename NT, bool useLock, bool useHashLock>
// template<void ETS(NT*, NT*, NT*, NT*, int, fusion_metadata)>
bool BTState<NT, useLock, useHashLock>::split_if_needed(void ETS(NT*, NT*, NT*, NT*, int, fusion_metadata)) {
    if(node_full(&cur->fusion_internal_tree)) {
        if(!try_upgrade_reverse_order()) { //Somewhat misleading but basically to tell you that you need to restart the inserting process
            // std::cout << "FDFSD" << std::endl;
            return true;
        }
        if(!split_node(ETS)) { //Return statement is just equal to par != NULL so maybe don't bother with this return statement? Can simplfiy ex next couple lines?
            // std::cout << "FDFSD2" << std::endl;
            // if constexpr (useLock) 
            //     write_unlock(&curMtx);
            if constexpr (useLock) 
                curMtx.writeUnlock();
            // std::cout << "FDFSD3" << std::endl;
        }
        else {
            // std::cout << "WHAT" << std::endl;
            if constexpr (useLock) 
                curMtx.writeUnlock();
            delete cur;
        }
        if constexpr (useLock) {
            // if(par != NULL)
            //     write_unlock(&parMtx);
            if(par != NULL)
                parMtx.writeUnlock();
        }
        return true;
    }
    return false; //Tells you split was unnecessary
}

template<typename NT, bool useLock, bool useHashLock>
// template<bool HP>
bool BTState<NT, useLock, useHashLock>::try_insert_key(__m512i key, bool auto_unlock) {
    if(try_upgrade_reverse_order()) {
        insert_key_node(&cur->fusion_internal_tree, key);
        if (auto_unlock)
            write_unlock_both();
        return true;
    }
    return false;
}

template<typename NT, bool useLock, bool useHashLock>
// template<bool HP>
bool BTState<NT, useLock, useHashLock>::try_HOH_readlock(NT* child) {
    if constexpr (useLock) {
        // if(!read_lock(&child->mtx, TRY_ONCE_LOCK, thread_id)) {
        //     read_unlock_both();
        //     return false;
        // }
        // if(par != NULL)
        //     read_unlock(&parMtx, thread_id);
        depth++;
        // HashMutex childMtx{lockTable, (size_t) child};
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