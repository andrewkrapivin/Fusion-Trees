#include "FusionBTree.h"
#include "HelperFuncs.h"
#include <iostream>
#include <cstring>
#include <assert.h>
#include <bitset>
#include "BTreeHelper.h"

uint64_t IDCounter = 0;

fusion_b_node* new_empty_node() {
    fusion_b_node* new_node = new fusion_b_node();
    //memset(new_node, 0, sizeof(fusion_b_node));
    new_node->id = IDCounter++;
    return new_node;
}

fusion_b_node* search_key_full_tree(fusion_b_node* root, __m512i key) {
    int branch = query_branch_node(&root->fusion_internal_tree, key);
    if(branch < 0 || root->children[branch] == NULL) { // either key exact match found or nowhere down to go
        return root;
    }
    return search_key_full_tree(root->children[branch], key);
}

fusion_b_node* insert_full_tree(fusion_b_node* root, __m512i key) {
    if(root == NULL) {
        root = new_empty_node();
        insert_key_node(&root->fusion_internal_tree, key);
        return root;
    }

    fusion_b_node* key_node = search_key_full_tree(root, key);
    if(!node_full(&key_node->fusion_internal_tree)) {
        insert_key_node(&key_node->fusion_internal_tree, key);
        return root;
    }
    __m512i pmedian = key;
    fusion_b_node* plefthalf = NULL;
    fusion_b_node* prighthalf = NULL;
    while(node_full(&key_node->fusion_internal_tree)) {
        fusion_node* key_fnode = &key_node->fusion_internal_tree;
        int keypos = query_branch_node(key_fnode, pmedian);
        if(keypos < 0) //already in the tree. Should only be true on the first loop time. If something breaks and that isnt the case, this will be bad
            return root;
        __m512i newmedian;
        fusion_b_node* newlefthalf = new_empty_node();
        fusion_b_node* newrighthalf = new_empty_node();
        
        //just adding half the keys to the left size and half to the right
        for(int i=0, j=0; i < MAX_FUSION_SIZE+1; i++) {
            fusion_b_node* curchild = i == keypos ? plefthalf : (i == (keypos+1) ? prighthalf : key_node->children[j]); //p hacky, probably fix this
            __m512i curkey = i == keypos ? pmedian : get_key_from_sorted_pos(key_fnode, j++);// and this
            if (i < MAX_FUSION_SIZE/2) {
                insert_key_node(&newlefthalf->fusion_internal_tree, curkey);
                newlefthalf->children[i] = curchild;
                if(curchild != NULL)
	                curchild->parent = newlefthalf;
            }
            else if(i == MAX_FUSION_SIZE/2) {
                newmedian = curkey;
                newlefthalf->children[i] = curchild;
				if(curchild != NULL)
	                curchild->parent = newlefthalf;
            }
            else {
                insert_key_node(&newrighthalf->fusion_internal_tree, curkey);
                newrighthalf->children[i-MAX_FUSION_SIZE/2-1] = curchild;
                if(curchild != NULL)
                	curchild->parent = newrighthalf;
            }
        }
        { //one extra branch than keys, so add this child
            fusion_b_node* curchild = (MAX_FUSION_SIZE+1) == (keypos+1) ? prighthalf : key_node->children[MAX_FUSION_SIZE];
            newrighthalf->children[MAX_FUSION_SIZE/2] = curchild;
            if(curchild != NULL)
	            curchild->parent = newrighthalf;
        }
        fusion_b_node* key_node_par = key_node->parent;
        if(key_node->fusion_internal_tree.tree.meta.fast) {
            make_fast(&newlefthalf->fusion_internal_tree);
            make_fast(&newrighthalf->fusion_internal_tree);
        }
        //I HAVE NO IDEA IF THIS IF STATEMENT SHOULD BE HERE. Wrote this a while ago oops. Now I really see the point of not doing these kinds of if statements lol
        if(key_node_par != NULL)
        free(key_node);
        key_node = key_node_par;
        if(key_node == NULL) { // went "above root," then we want to create new root
            root = new_empty_node();
            key_node = root;
        }
        pmedian = newmedian;
        plefthalf = newlefthalf;
        prighthalf = newrighthalf;
    }
    
    insert_key_node(&key_node->fusion_internal_tree, pmedian);
    if(!key_node->fusion_internal_tree.tree.meta.fast) {make_fast(&key_node->fusion_internal_tree);}
    
    fusion_node tmp = {0};
    for(int i=0; i < key_node->fusion_internal_tree.tree.meta.size; i++) {
        insert(&tmp, key_node->fusion_internal_tree.keys[i]);
    }

    int npos = ~query_branch_node(&key_node->fusion_internal_tree, pmedian);
    for(int i=key_node->fusion_internal_tree.tree.meta.size /*ok that is ridiculous, fix that*/; i >= npos+2; i--) {
        key_node->children[i] = key_node->children[i-1];
    }

    key_node->children[npos] = plefthalf;
    key_node->children[npos+1] = prighthalf;
    plefthalf->parent = key_node;
    prighthalf->parent = key_node;
    
    return root;
}

__m512i* successor(fusion_b_node* root, __m512i key, bool foundkey=false, bool needbig=false) { //returns null if there is no successor
    if(root == NULL) return NULL;
    __m512i* retval = NULL;
    fusion_b_node* cur = root;
    while(true) {
        int branch = query_branch_node(&cur->fusion_internal_tree, key);
        if(branch < 0) {
            branch = (~branch) + 1;
        }
        if(cur->fusion_internal_tree.tree.meta.size > branch) {
            retval = &cur->fusion_internal_tree.keys[get_real_pos_from_sorted_pos(&cur->fusion_internal_tree, branch)];
        }
        if(cur->children[branch] == NULL) {
            return retval;
        }
        cur = cur->children[branch];
    }
}

//TODO: make sure the following predecessor functions are correct (I just quickly modified them from successor)
__m512i* predecessor(fusion_b_node* root, __m512i key, bool foundkey=false, bool needbig=false) { //returns null if there is no successor
    if(root == NULL) return NULL;
    __m512i* retval = NULL;
    fusion_b_node* cur = root;
    while(true) {
        int branch = query_branch_node(&cur->fusion_internal_tree, key);
        if(branch < 0) {
            branch = (~branch) ;
        }
        if(branch > 0) {
            retval = &cur->fusion_internal_tree.keys[get_real_pos_from_sorted_pos(&cur->fusion_internal_tree, branch-1)];
        }
        if(cur->children[branch] == NULL) {
            return retval;
        }
        cur = cur->children[branch];
    }
}

void printTree(fusion_b_node* root, int indent=0) {
	if(root->visited) {
		for(int i = 0; i < indent; i++) cout << " ";
		cout << "Strange. Already visited node " << root->id << endl;
		return;
	}
	root->visited = true;
	int branchcount = 0;
	for(int i = 0; i < MAX_FUSION_SIZE+1; i++) {
		if(root->children[i] != NULL) {
			branchcount++;
        }
	}
	for(int i = 0; i < indent; i++) cout << " ";
	if(root->parent != NULL)
		cout << "Node " << root->id << " has " << root->parent->id << " as a parent." << endl;
	for(int i = 0; i < indent; i++) cout << " ";
    // cout << "isfull: " << node_full(&root->fusion_internal_tree) << ", ";
	cout << "Node " << root->id << " has " << branchcount << " children (size of fusion node: " << (int)root->fusion_internal_tree.tree.meta.size << ") Exploring them now: {" << endl;
	for(int i = 0; i < MAX_FUSION_SIZE+1; i++) {
		if(root->children[i] != NULL) {
			printTree(root->children[i], indent+4);
        }
	}
	root->visited = false;
	for(int i = 0; i < indent; i++) cout << " ";
	cout << "}" << endl;
}

int maxDepth(fusion_b_node* root) {
    int ans = 0;
	for(int i = 0; i < MAX_FUSION_SIZE+1; i++) {
		if(root->children[i] != NULL)
			ans = max(maxDepth(root->children[i])+1, ans);
	}
    return ans;
}

size_t numNodes(fusion_b_node* root) {
    size_t num = 1;
	for(int i = 0; i < MAX_FUSION_SIZE+1; i++) {
		if(root->children[i] != NULL)
			num += numNodes(root->children[i]);
	}
    return num;
}

size_t totalDepth(fusion_b_node* root, size_t dep = 0) {
    size_t num = dep;
	for(int i = 0; i < MAX_FUSION_SIZE+1; i++) {
		if(root->children[i] != NULL)
			num += totalDepth(root->children[i], dep+1);
	}
    return num;
}

size_t memUsage(fusion_b_node* root) { // in MB
    return numNodes(root)*sizeof(fusion_b_node);
}

FusionBTree::FusionBTree() {
    root = NULL;
}

void FusionBTree::insert(__m512i key) {
    root = ::insert_full_tree(root, key);
}

__m512i* FusionBTree::successor(__m512i key) {
    return ::successor(root, key);
}

__m512i* FusionBTree::predecessor(__m512i key) {
    return ::predecessor(root, key);
}

void FusionBTree::printTree() {
    ::printTree(root);
}

int FusionBTree::maxDepth() {
    return ::maxDepth(root);
}

size_t FusionBTree::numNodes() {
    return ::numNodes(root);
}

size_t FusionBTree::totalDepth() {
    return ::totalDepth(root);
}

size_t FusionBTree::memUsage() {
    return ::memUsage(root);
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

typedef BTState<parallel_fusion_b_node, true> PBState;

//keep track of how many times we "restart" in the tree
void parallel_insert_full_tree_DLock(parallel_fusion_b_node* root, __m512i key, uint8_t thread_id) {
    // assert(root != NULL);

    PBState state(root, thread_id);

    while(true) {
        if(state.split_if_needed()) {
            return parallel_insert_full_tree_DLock(root, key, thread_id);
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
            return parallel_insert_full_tree_DLock(root, key, thread_id);
        }
        if(!state.try_HOH_readlock(state.cur->children[branch])) {
            return parallel_insert_full_tree_DLock(root, key, thread_id);
        }
    }
}

//Is this function even tested?
__m512i* parallel_successor_DLock(parallel_fusion_b_node* root, __m512i key, uint8_t thread_id) { //returns null if there is no successor
    __m512i* retval = NULL;
    PBState state(root, thread_id);
    
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
            return parallel_successor_DLock(root, key, thread_id);
        }
    }
}

//This function is 100% not tested, kinda just copied and modified successor
__m512i* parallel_predecessor_DLock(parallel_fusion_b_node* root, __m512i key, uint8_t thread_id) { //returns null if there is no successor
    __m512i* retval = NULL;
    PBState state(root, thread_id);

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
            return parallel_predecessor_DLock(root, key, thread_id);
        }
    }
}



void ParallelFusionBTree::insert(__m512i key) {
    parallel_insert_full_tree_DLock(root, key, thread_id);
}

__m512i* ParallelFusionBTree::successor(__m512i key) {
    return parallel_successor_DLock(root, key, thread_id);
}

__m512i* ParallelFusionBTree::predecessor(__m512i key) {
    return parallel_predecessor_DLock(root, key, thread_id);
}