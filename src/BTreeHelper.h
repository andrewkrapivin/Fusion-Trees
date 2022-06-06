#include <immintrin.h>
#include <cstdint>

#include "lock.h"
#include "fusion_tree.h"

//TODO: generalize this template for any kind of B-tree so that could do some comparative testing.

template<typename NT> //NT--node type.
//NT MUST have fusion_internal_tree, children, mtx somewhere in it. I don't know how to restrict it so that that is the case, but just this is a thing. Basically, it must be related to parallel_fusion_b_node, but it can also be the variable size one
//Another thing that must be guaranteed, and this is definitely sus, is that we have the fusion node then the children arrayed next to each other in memory for clearing.
//Honestly maybe figure out a better system for this
struct BTState {
    // template<bool HP>
    // static void emptyfunc(NT* cur, NT* lc, NT* rc) {
    // }
    static void emptyfunc(NT* par, NT* cur, NT* lc, NT* rc, int medpos, fusion_metadata old_meta) {
    }

    NT* cur;
    NT* par;
    uint8_t thread_id;
    // template<bool HP> NT (*extra_splitting)(NT*, NT*, NT*);
    BTState(NT* root, uint8_t thread_id);
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
};




template<typename NT>
BTState<NT>::BTState(NT* root, uint8_t thread_id): cur(root), par(NULL), thread_id(thread_id) {
    read_lock(&cur->mtx, WAIT_FOR_LOCK, thread_id);
}

template<typename NT>
// template<bool HP>
bool BTState<NT>::try_upgrade_reverse_order() {
    if(!partial_upgrade(&cur->mtx, TRY_ONCE_LOCK, thread_id)) {
        // if(HP)
        if(par != NULL)
            read_unlock(&par->mtx, thread_id);
        return false;
    }
    if (par == NULL) {
        finish_partial_upgrade(&cur->mtx);
        return true;
    }
    if(!partial_upgrade(&par->mtx, TRY_ONCE_LOCK, thread_id)) {
        unlock_partial_upgrade(&cur->mtx);
        return false;
    }
    finish_partial_upgrade(&par->mtx);
    finish_partial_upgrade(&cur->mtx);
    return true;
}

template<typename NT>
// template<bool HP>
void BTState<NT>::read_unlock_both() {
    read_unlock(&cur->mtx, thread_id);
    if(par != NULL) read_unlock(&par->mtx, thread_id);
}

template<typename NT>
// template<bool HP>
void BTState<NT>::write_unlock_both() {
    write_unlock(&cur->mtx);
    if(par != NULL) write_unlock(&par->mtx);
}

template<typename NT>
// template<void ETS(NT*, NT*, NT*, NT*, int, fusion_metadata)> //Why does this templated version not work??? It should...
bool BTState<NT>::split_node(void ETS(NT*, NT*, NT*, NT*, int, fusion_metadata)) {
    fusion_node* key_fnode = &cur->fusion_internal_tree;

    NT* newlefthalf = new NT();
    NT* newrighthalf = new NT();

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

template<typename NT>
// template<void ETS(NT*, NT*, NT*, NT*, int, fusion_metadata)>
bool BTState<NT>::split_if_needed(void ETS(NT*, NT*, NT*, NT*, int, fusion_metadata)) {
    if(node_full(&cur->fusion_internal_tree)) {
        if(!try_upgrade_reverse_order()) { //Somewhat misleading but basically to tell you that you need to restart the inserting process
            return true;
        }
        if(!split_node(ETS)) { //Return statement is just equal to par != NULL so maybe don't bother with this return statement? Can simplfiy ex next couple lines?
            write_unlock(&cur->mtx);
        }
        else {
            delete cur;
        }
        if(par != NULL)
            write_unlock(&par->mtx);
        return true;
    }
    return false; //Tells you split was unnecessary
}

template<typename NT>
// template<bool HP>
bool BTState<NT>::try_insert_key(__m512i key, bool auto_unlock) {
    if(try_upgrade_reverse_order()) {
        insert_key_node(&cur->fusion_internal_tree, key);
        if (auto_unlock)
            write_unlock_both();
        return true;
    }
    return false;
}

template<typename NT>
// template<bool HP>
bool BTState<NT>::try_HOH_readlock(NT* child) {
    if(!read_lock(&child->mtx, TRY_ONCE_LOCK, thread_id)) {
        read_unlock_both();
        return false;
    }
    if(par != NULL)
        read_unlock(&par->mtx, thread_id);
    par = cur;
    cur = child;
    return true;
}