#include "FusionBTree.h"
#include "HelperFuncs.h"
#include <iostream>
#include <cstring>

uint64_t IDCounter = 0;

fusion_b_node* new_empty_node(SimpleAlloc<fusion_b_node, 64>& allocator) {
	//cout << sizeof(fusion_b_node) << endl;
    fusion_b_node* new_node = allocator.alloc();
    memset(new_node, 0, sizeof(fusion_b_node));
    new_node->id = IDCounter++;
    return new_node;
}

fusion_b_node* search_key_full_tree(fusion_b_node* root, __m512i key) {
    int branch = query_branch(&root->fusion_internal_tree, key);
    //cout << "branching " << branch << endl;
    if(branch < 0 || root->children[branch] == NULL) { // either key exact match found or nowhere down to go
        branch = ~branch;
        return root;
    }
    return search_key_full_tree(root->children[branch], key);
}

fusion_b_node* insert_full_tree(fusion_b_node* root, __m512i key, SimpleAlloc<fusion_b_node, 64>& allocator) {
    if(root == NULL) {
        root = new_empty_node(allocator);
        //cout << "HELLO " << &root->fusion_internal_tree << endl;
        insert(&root->fusion_internal_tree, key);
        //cout << "HELLO" << endl;
        return root;
    }

    fusion_b_node* key_node = search_key_full_tree(root, key);
    if(!node_full(&key_node->fusion_internal_tree)) {
        insert(&key_node->fusion_internal_tree, key);
        return root;
    }
    __m512i pmedian = key;
    fusion_b_node* plefthalf = NULL;
    fusion_b_node* prighthalf = NULL;
    //printTree(root);
    while(node_full(&key_node->fusion_internal_tree)) {
    	//print_keys_sig_bits(&key_node->fusion_internal_tree);
        fusion_node* key_fnode = &key_node->fusion_internal_tree;
        int keypos = query_branch(key_fnode, pmedian);
        if(keypos < 0) //already in the tree. Should only be true on the first loop time. If something breaks and that isnt the case, this will be bad
            return root;
        __m512i newmedian;
        fusion_b_node* newlefthalf = new_empty_node(allocator);
        fusion_b_node* newrighthalf = new_empty_node(allocator);
        //just adding half the keys to the left size and half to the right
        for(int i=0, j=0; i < MAX_FUSION_SIZE+1; i++) {
            fusion_b_node* curchild = i == keypos ? plefthalf : (i == (keypos+1) ? prighthalf : key_node->children[j]); //p hacky, probably fix this
            /*if(curchild != NULL) {
            	cout << "curchild is " << curchild->id << endl;
            }*/
            __m512i curkey = i == keypos ? pmedian : get_key_from_sorted_pos(key_fnode, j++);// and this
            if (i < MAX_FUSION_SIZE/2) {
                insert(&newlefthalf->fusion_internal_tree, curkey);
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
                insert(&newrighthalf->fusion_internal_tree, curkey);
                newrighthalf->children[i-MAX_FUSION_SIZE/2-1] = curchild;
                if(curchild != NULL)
                	curchild->parent = newrighthalf;
            }
        }
        { //one extra branch than keys, so add this child
            fusion_b_node* curchild = (MAX_FUSION_SIZE+1) == (keypos+1) ? prighthalf : key_node->children[MAX_FUSION_SIZE];
            newrighthalf->children[MAX_FUSION_SIZE/2] = curchild;
            /*if(curchild != NULL) {
            	cout << "curchild is " << curchild->id << endl;
            }*/
            if(curchild != NULL)
	            curchild->parent = newrighthalf;
        }
        fusion_b_node* key_node_par = key_node->parent;
        //printTree(root);
        //cout << "Removing node " << key_node->id << endl;
        allocator.free(key_node);
        key_node = key_node_par;
        if(key_node == NULL) { // went "above root," then we want to create new root
            root = new_empty_node(allocator);
            key_node = root;
        }
        pmedian = newmedian;
        plefthalf = newlefthalf;
        prighthalf = newrighthalf;
        /*cout << "printing new left half" << endl;
        print_keys_sig_bits(&newlefthalf->fusion_internal_tree);
        cout << "printing new right half" << endl;
        print_keys_sig_bits(&newrighthalf->fusion_internal_tree);
        cout << "New median: " << endl;
        print_binary_uint64_big_endian(pmedian[7], true, 64, 8);*/
    }
    //printTree(root);
	//cout << ((int)plefthalf->fusion_internal_tree.tree.meta.size) << " " << ((int)prighthalf->fusion_internal_tree.tree.meta.size) << endl;
    insert(&key_node->fusion_internal_tree, pmedian);
    //cout << "New things" << endl;
    //print_keys_sig_bits(&key_node->fusion_internal_tree);
    int npos = ~query_branch(&key_node->fusion_internal_tree, pmedian);
    //shifting to make room for new children (& we want to ignore the actual child that will be replaced by the two children, which should be at the position of the new key)
    //The child we want to replace is exactly at the position of the added median, since it was to the "right" of the key smaller than the median and to the "left" of the key larger than the median, so its position is one more than the position of the key to the "left" of the median now, so the position of the median. Thus we want to ignore that key and move the things to the right of that by one
    //cout << "Npos is " << npos << endl;
    //cout << "Key_node id is " << key_node->id << endl;
    for(int i=key_node->fusion_internal_tree.tree.meta.size /*ok that is ridiculous, fix that*/; i >= npos+2; i--) {
        key_node->children[i] = key_node->children[i-1];
    }
    key_node->children[npos] = plefthalf;
    key_node->children[npos+1] = prighthalf;
    plefthalf->parent = key_node;
    prighthalf->parent = key_node;
    //cout << "plefthalf id is " << plefthalf->id << ", prighthalf id is " << prighthalf->id << ", and key_node id is " << key_node->id << endl;
    
    return root;
}

__m512i* successor(fusion_b_node* root, __m512i key, bool foundkey /*=false*/, bool needbig/*=false*/) { //returns null if there is no successor
	if(root == NULL) return NULL;
	if(foundkey) {
		//cout << "Found key" << endl;
		int branch = needbig ? root->fusion_internal_tree.tree.meta.size : 0;
		__m512i* ans = successor(root->children[branch], key, true, needbig);
		branch = needbig ? (root->fusion_internal_tree.tree.meta.size-1) : 0;
		return ans == NULL ? &root->fusion_internal_tree.keys[get_real_pos_from_sorted_pos(&root->fusion_internal_tree, branch)] : ans;
	}
    int branch = query_branch(&root->fusion_internal_tree, key);
    //print_keys_sig_bits(&root->fusion_internal_tree);
    //cout << "Branch is " << branch << ", and is it null: " << (root->children[branch < 0 ? 0 : branch] == NULL) << endl;
    if(branch < 0) { // exact key match found
        branch = ~branch;
        if(root->children[branch+1] != NULL) { //This was root->children[branch] before, but that caused no problems for some reason? wtf? Oh, maybe cause its a B-tree that just never really happens. Yeah I think if there's just one child then that's enough
	        return successor(root->children[branch+1], key, true, false);
	    }
	    else if(branch+1 < root->fusion_internal_tree.tree.meta.size) {
	    	return &root->fusion_internal_tree.keys[get_real_pos_from_sorted_pos(&root->fusion_internal_tree, branch+1)];
	   	}
	    return NULL;
    }
    __m512i* ans;
    if(root->children[branch] == NULL || (ans = successor(root->children[branch], key)) == NULL){ //when didn't find the successor below, we now look if the successor is here
    	if(branch < root->fusion_internal_tree.tree.meta.size) {
    		return &root->fusion_internal_tree.keys[get_real_pos_from_sorted_pos(&root->fusion_internal_tree, branch)];
    	}
    	else return NULL;
    }
    return ans;
}

__m512i* predecessor(fusion_b_node* root, __m512i key, bool foundkey /*=false*/, bool needbig/*=false*/) { //returns null if there is no successor
	if(root == NULL) return NULL;
	if(foundkey) {
		//cout << "Found key" << endl;
		int branch = needbig ? root->fusion_internal_tree.tree.meta.size : 0;
		__m512i* ans = predecessor(root->children[branch], key, true, needbig);
		branch = needbig ? (root->fusion_internal_tree.tree.meta.size-1) : 0;
		return ans == NULL ? &root->fusion_internal_tree.keys[get_real_pos_from_sorted_pos(&root->fusion_internal_tree, branch)] : ans;
	}
    int branch = query_branch(&root->fusion_internal_tree, key);
    //print_keys_sig_bits(&root->fusion_internal_tree);
    //cout << "Branch is " << branch << ", and is it null: " << (root->children[branch < 0 ? 0 : branch] == NULL) << endl;
    if(branch < 0) { // exact key match found
        branch = ~branch;
        if(root->children[branch] != NULL) {
	        return predecessor(root->children[branch], key, true, true);
	    }
	    else if(branch-1 >= 0) {
	    	return &root->fusion_internal_tree.keys[get_real_pos_from_sorted_pos(&root->fusion_internal_tree, branch-1)];
	   	}
	    return NULL;
    }
    __m512i* ans;
    if(root->children[branch] == NULL || (ans = predecessor(root->children[branch], key)) == NULL){ //when didn't find the successor below, we now look if the successor is here
    	if(branch-1 >= 0) {
    		return &root->fusion_internal_tree.keys[get_real_pos_from_sorted_pos(&root->fusion_internal_tree, branch-1)];
    	}
    	else return NULL;
    }
    return ans;
}

void printTree(fusion_b_node* root, int indent) {
	if(root->visited) {
		for(int i = 0; i < indent; i++) cout << " ";
		cout << "Strange. Already visited node " << root->id << endl;
		return;
	}
	root->visited = true;
	int branchcount = 0;
	for(int i = 0; i < MAX_FUSION_SIZE+1; i++) {
		if(root->children[i] != NULL)
			branchcount++;
	}
	for(int i = 0; i < indent; i++) cout << " ";
	if(root->parent != NULL)
		cout << "Node " << root->id << " has " << root->parent->id << " as a parent." << endl;
	for(int i = 0; i < indent; i++) cout << " ";
	cout << "Node " << root->id << " has " << branchcount << " children. Exploring them now: {" << endl;
	for(int i = 0; i < MAX_FUSION_SIZE+1; i++) {
		if(root->children[i] != NULL)
			printTree(root->children[i], indent+4);
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

size_t totalDepth(fusion_b_node* root, size_t dep) {
    size_t num = dep;
	for(int i = 0; i < MAX_FUSION_SIZE+1; i++) {
		if(root->children[i] != NULL)
			num += totalDepth(root->children[i], dep+1);
	}
    return num;
}

size_t memUsage(fusion_b_node* root) { // in MB
    return numNodes(root)*sizeof(fusion_b_node)/1000000;
}