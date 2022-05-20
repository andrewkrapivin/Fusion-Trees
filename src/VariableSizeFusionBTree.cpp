#include "VariableSizeFusionBTree.h"
#include "HelperFuncs.h"
#include <iostream>
#include <cstring>
#include <assert.h>
#include <bitset>

template<typename VT>
static void deletepnode(vsize_parallel_fusion_b_node* node) {
    pc_destructor(&node->mtx.pc_counter);
    free(node);
}

template<typename VT>
static bool split_node_DLock(vsize_parallel_fusion_b_node* node, vsize_parallel_fusion_b_node* par, uint8_t thread_id) {
    fusion_node* key_fnode = &node->fusion_internal_tree;

    vsize_parallel_fusion_b_node* newlefthalf = new vsize_parallel_fusion_b_node();
    rw_lock_init(&newlefthalf->mtx);
    vsize_parallel_fusion_b_node* newrighthalf = new vsize_parallel_fusion_b_node();
    rw_lock_init(&newrighthalf->mtx);

    constexpr int medpos = MAX_FUSION_SIZE/2; //two choices since max size even: maybe randomize?
    for(int i = 0; i < medpos; i++) {
        insert(&newlefthalf->fusion_internal_tree, get_key_from_sorted_pos(&node->fusion_internal_tree, i));
        newlefthalf->children[i] = node->children[i];
    }
    newlefthalf->children[medpos] = node->children[medpos];
    for(int i = medpos+1; i < MAX_FUSION_SIZE; i++) {
        insert(&newrighthalf->fusion_internal_tree, get_key_from_sorted_pos(&node->fusion_internal_tree, i));
        newrighthalf->children[i-medpos-1] = node->children[i];
    }
    newrighthalf->children[MAX_FUSION_SIZE-medpos-1] = node->children[MAX_FUSION_SIZE];

    if(node->fusion_internal_tree.tree.meta.fast) {
        make_fast(&newlefthalf->fusion_internal_tree);
        make_fast(&newrighthalf->fusion_internal_tree);
    }

    __m512i median = get_key_from_sorted_pos(&node->fusion_internal_tree, medpos);

    if(par == NULL) {
        memset(&node->fusion_internal_tree, 0, sizeof(fusion_node) + sizeof(vsize_parallel_fusion_b_node*)*(MAX_FUSION_SIZE+1));
        insert_key_node(&node->fusion_internal_tree, median);
        node->children[0] = newlefthalf;
        node->children[1] = newrighthalf;
        return false;
    }

    // node->deleted = true;
    // free(node);
    insert_key_node(&par->fusion_internal_tree, median);
    int pos = ~query_branch_node(&par->fusion_internal_tree, median);
    for(int i=par->fusion_internal_tree.tree.meta.size /*ok that is ridiculous, fix that*/; i >= pos+2; i--) {
        par->children[i] = par->children[i-1];
    }
    par->children[pos] = newlefthalf;
    par->children[pos+1] = newrighthalf;
    return true;
}

template<typename VT>
bool try_upgrade_reverse_order_DLock(vsize_parallel_fusion_b_node* child, vsize_parallel_fusion_b_node* par, uint8_t thread_id) {
    if(!partial_upgrade(&child->mtx, TRY_ONCE_LOCK, thread_id)) {
        read_unlock(&par->mtx, thread_id);
        return false;
    }
    if(!partial_upgrade(&par->mtx, TRY_ONCE_LOCK, thread_id)) {
        unlock_partial_upgrade(&child->mtx);
        return false;
    }
    finish_partial_upgrade(&par->mtx);
    finish_partial_upgrade(&child->mtx);
    return true;
}

//keep track of how many times we "restart" in the tree
template<typename VT>
void vsize_parallel_insert_full_tree_DLock(vsize_parallel_fusion_b_node* root, __m512i key, uint8_t thread_id) {
    // assert(root != NULL);

    vsize_parallel_fusion_b_node* par = NULL;
    vsize_parallel_fusion_b_node* cur = root;
    read_lock(&cur->mtx, WAIT_FOR_LOCK, thread_id);

    if(node_full(&cur->fusion_internal_tree)) {
        if(partial_upgrade(&cur->mtx, TRY_ONCE_LOCK, thread_id)) {
            finish_partial_upgrade(&cur->mtx);
            if(!split_node_DLock<VT>(cur, par, thread_id)) {
                write_unlock(&cur->mtx);
            }
            else {
                deletepnode<VT>(cur);
            }
        }
        return vsize_parallel_insert_full_tree_DLock<VT>(root, key, thread_id);
    }
    int branch = query_branch_node(&cur->fusion_internal_tree, key);
    if(branch < 0) { //say exact match just return & do nothing
        read_unlock(&cur->mtx, thread_id);
        return;
    }
    if(cur->children[branch] == NULL) {
        if(partial_upgrade(&cur->mtx, TRY_ONCE_LOCK, thread_id)) {
            finish_partial_upgrade(&cur->mtx);
            if(node_full(&cur->fusion_internal_tree)) {
                split_node_DLock<VT>(cur, par, thread_id);
                if(!split_node_DLock<VT>(cur, par, thread_id)) {
                    write_unlock(&cur->mtx);
                }
                else {
                    deletepnode<VT>(cur);
                }
                return vsize_parallel_insert_full_tree_DLock<VT>(root, key, thread_id);
            }
            insert_key_node(&cur->fusion_internal_tree, key);
            write_unlock(&cur->mtx);
            return;
        }
        return vsize_parallel_insert_full_tree_DLock<VT>(root, key, thread_id);
    }
    par = cur;
    cur = cur->children[branch];
    
    if(!read_lock(&cur->mtx, TRY_ONCE_LOCK, thread_id)) {
        read_unlock(&par->mtx, thread_id);
        return vsize_parallel_insert_full_tree_DLock<VT>(root, key, thread_id);
    }
    
    while(true) { //assumes par and cur exist and are locked according to the paradigm.
        if(node_full(&cur->fusion_internal_tree)) {
            if(!try_upgrade_reverse_order_DLock<VT>(cur, par, thread_id)) {
                return vsize_parallel_insert_full_tree_DLock<VT>(root, key, thread_id);
            }
            if(!split_node_DLock<VT>(cur, par, thread_id)) {
                write_unlock(&cur->mtx);
            }
            else {
                deletepnode<VT>(cur);
            }
            write_unlock(&par->mtx);
            return vsize_parallel_insert_full_tree_DLock<VT>(root, key, thread_id);
        }
        int branch = query_branch_node(&cur->fusion_internal_tree, key);
        if(branch < 0) { //say exact match just return & do nothing
            read_unlock(&par->mtx, thread_id);
            read_unlock(&cur->mtx, thread_id);
            return;
        }
        if(cur->children[branch] == NULL) {
            if(!try_upgrade_reverse_order_DLock<VT>(cur, par, thread_id)) {
                return vsize_parallel_insert_full_tree_DLock<VT>(root, key, thread_id);
            }
            insert_key_node(&cur->fusion_internal_tree, key);
            write_unlock(&par->mtx);
            write_unlock(&cur->mtx);
            return;
        }
        vsize_parallel_fusion_b_node* nchild = cur->children[branch];
        if(!read_lock(&nchild->mtx, TRY_ONCE_LOCK, thread_id)) {
            read_unlock(&par->mtx, thread_id);
            read_unlock(&cur->mtx, thread_id);
            return vsize_parallel_insert_full_tree_DLock<VT>(root, key, thread_id);
        }
        read_unlock(&par->mtx, thread_id);
        par = cur;
        cur = nchild;
    }
}


template<typename VT>
__m512i* vsize_parallel_successor_DLock(vsize_parallel_fusion_b_node* root, __m512i key, uint8_t thread_id) { //returns null if there is no successor
    __m512i* retval = NULL;
    vsize_parallel_fusion_b_node* cur = root;
    vsize_parallel_fusion_b_node* par = NULL;
    read_lock(&cur->mtx, WAIT_FOR_LOCK, thread_id);
    int branch = query_branch_node(&cur->fusion_internal_tree, key);
    if(branch < 0) {
        branch = (~branch) + 1;
    }
    if(cur->fusion_internal_tree.tree.meta.size > branch) {
        retval = &cur->fusion_internal_tree.keys[get_real_pos_from_sorted_pos(&cur->fusion_internal_tree, branch)];
    }
    if(cur->children[branch] == NULL) {
        read_unlock(&cur->mtx, thread_id);
        return retval;
    }
    par = cur;
    cur = par->children[branch];
    
    if(!read_lock(&cur->mtx, TRY_ONCE_LOCK, thread_id)) {
        read_unlock(&par->mtx, thread_id);
        return vsize_parallel_successor_DLock<VT>(root, key, thread_id);
    }
    while(true) { //assumes par and cur exist and are locked according to the paradigm.
        int branch = query_branch_node(&cur->fusion_internal_tree, key);
        if(branch < 0) {
            branch = (~branch) + 1;
        }
        if(cur->fusion_internal_tree.tree.meta.size > branch) {
            retval = &cur->fusion_internal_tree.keys[get_real_pos_from_sorted_pos(&cur->fusion_internal_tree, branch)];
        }
        if(cur->children[branch] == NULL) {
            read_unlock(&par->mtx, thread_id);
            read_unlock(&cur->mtx, thread_id);
            return retval;
        }
        if(!read_lock(&cur->children[branch]->mtx, TRY_ONCE_LOCK, thread_id)) {
            read_unlock(&par->mtx, thread_id);
            read_unlock(&cur->mtx, thread_id);
            return vsize_parallel_successor_DLock<VT>(root, key, thread_id);
        }
        read_unlock(&par->mtx, thread_id);
        par = cur;
        cur = cur->children[branch];
    }   
}

template<typename VT>
__m512i* vsize_parallel_predecessor_DLock(vsize_parallel_fusion_b_node* root, __m512i key, uint8_t thread_id) { //returns null if there is no successor
    __m512i* retval = NULL;
    vsize_parallel_fusion_b_node* cur = root;
    vsize_parallel_fusion_b_node* par = NULL;
    read_lock(&cur->mtx, WAIT_FOR_LOCK, thread_id);
    int branch = query_branch_node(&cur->fusion_internal_tree, key);
    if(branch < 0) {
        branch = (~branch);
    }
    if(branch > 0) {
        retval = &cur->fusion_internal_tree.keys[get_real_pos_from_sorted_pos(&cur->fusion_internal_tree, branch-1)];
    }
    if(cur->children[branch] == NULL) {
        read_unlock(&cur->mtx, thread_id);
        return retval;
    }
    par = cur;
    cur = par->children[branch];
    
    if(!read_lock(&cur->mtx, TRY_ONCE_LOCK, thread_id)) {
        read_unlock(&par->mtx, thread_id);
        return vsize_parallel_predecessor_DLock<VT>(root, key, thread_id);
    }
    while(true) { //assumes par and cur exist and are locked according to the paradigm.
        int branch = query_branch_node(&cur->fusion_internal_tree, key);
        if(branch < 0) {
            branch = (~branch);
        }
        if(branch > 0) {
            retval = &cur->fusion_internal_tree.keys[get_real_pos_from_sorted_pos(&cur->fusion_internal_tree, branch-1)];
        }
        if(cur->children[branch] == NULL) {
            read_unlock(&par->mtx, thread_id);
            read_unlock(&cur->mtx, thread_id);
            return retval;
        }
        if(!read_lock(&cur->children[branch]->mtx, TRY_ONCE_LOCK, thread_id)) {
            read_unlock(&par->mtx, thread_id);
            read_unlock(&cur->mtx, thread_id);
            return vsize_parallel_predecessor_DLock<VT>(root, key, thread_id);
        }
        read_unlock(&par->mtx, thread_id);
        par = cur;
        cur = cur->children[branch];
    }   
}


template<typename VT>
void VariableSizeParallelFusionBTree<VT>::insert(__m512i key) {
    vsize_parallel_insert_full_tree_DLock<VT>(root, key, thread_id);
}

template<typename VT>
__m512i* VariableSizeParallelFusionBTree<VT>::successor(__m512i key) {
    return vsize_parallel_successor_DLock<VT>(root, key, thread_id);
}

template<typename VT>
__m512i* VariableSizeParallelFusionBTree<VT>::predecessor(__m512i key) {
    return vsize_parallel_predecessor_DLock<VT>(root, key, thread_id);
}