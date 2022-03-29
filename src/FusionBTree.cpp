#include "FusionBTree.h"
#include "HelperFuncs.h"
#include <iostream>
#include <cstring>
#include <assert.h>
#include <bitset>

uint64_t IDCounter = 0;

fusion_b_node* new_empty_node(SimpleAlloc<fusion_b_node, 64>& allocator) {
	//cout << sizeof(fusion_b_node) << endl;
    fusion_b_node* new_node = allocator.alloc();
    memset(new_node, 0, sizeof(fusion_b_node));
    new_node->id = IDCounter++;
    // cout << "Allocated " << new_node->id << endl;
    return new_node;
}

fusion_b_node* search_key_full_tree(fusion_b_node* root, __m512i key) {
    int branch = query_branch_node(&root->fusion_internal_tree, key);
    if(branch < 0 || root->children[branch] == NULL) { // either key exact match found or nowhere down to go
        // branch = ~branch;
        return root;
    }
    return search_key_full_tree(root->children[branch], key);
}

fusion_b_node* insert_full_tree(fusion_b_node* root, __m512i key, SimpleAlloc<fusion_b_node, 64>& allocator) {
    if(root == NULL) {
        root = new_empty_node(allocator);
        //cout << "HELLO " << &root->fusion_internal_tree << endl;
        insert_key_node(&root->fusion_internal_tree, key);
        //cout << "HELLO" << endl;
        return root;
    }

    fusion_b_node* key_node = search_key_full_tree(root, key);
    // cout << "id: " << key_node->id << endl;
    if(!node_full(&key_node->fusion_internal_tree)) {
        insert_key_node(&key_node->fusion_internal_tree, key);
        return root;
    }
    __m512i pmedian = key;
    fusion_b_node* plefthalf = NULL;
    fusion_b_node* prighthalf = NULL;
    // printTree(root);
    while(node_full(&key_node->fusion_internal_tree)) {
        // print_keys_sig_bits(&key_node->fusion_internal_tree);
        fusion_node* key_fnode = &key_node->fusion_internal_tree;
        int keypos = query_branch_node(key_fnode, pmedian);
        // cout << "Keypos: " << keypos << ", and keynode is fast: "<< key_fnode->tree.meta.fast << endl;
        // print_binary_uint64_big_endian(pmedian[7], true, 64, 16);
        // print_binary_uint64_big_endian(get_key_from_sorted_pos(key_fnode, 0)[7], true, 64, 16);
        if(keypos < 0) //already in the tree. Should only be true on the first loop time. If something breaks and that isnt the case, this will be bad
            return root;
        __m512i newmedian;
        fusion_b_node* newlefthalf = new_empty_node(allocator);
        // print_binary_uint64_big_endian(get_key_from_sorted_pos(key_fnode, 0)[7], true, 64, 16);
        fusion_b_node* newrighthalf = new_empty_node(allocator);
        // print_binary_uint64_big_endian(get_key_from_sorted_pos(key_fnode, 0)[7], true, 64, 16);
        //just adding half the keys to the left size and half to the right
        for(int i=0, j=0; i < MAX_FUSION_SIZE+1; i++) {
            fusion_b_node* curchild = i == keypos ? plefthalf : (i == (keypos+1) ? prighthalf : key_node->children[j]); //p hacky, probably fix this
            // if(curchild != NULL) {
            // 	cout << "curchild is " << curchild->id << endl;
            // }
            // cout << "j is " << j << endl;
            // print_binary_uint64_big_endian(get_key_from_sorted_pos(key_fnode, j)[7], true, 64, 16);
            __m512i curkey = i == keypos ? pmedian : get_key_from_sorted_pos(key_fnode, j++);// and this
            // print_binary_uint64_big_endian(curkey[7], true, 64, 16);
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
            // if(curchild != NULL) {
            // 	cout << "curchild is " << curchild->id << endl;
            // }
            if(curchild != NULL)
	            curchild->parent = newrighthalf;
        }
        fusion_b_node* key_node_par = key_node->parent;
        // printTree(root);
        if(key_node->fusion_internal_tree.tree.meta.fast) {
            make_fast(&newlefthalf->fusion_internal_tree);
            make_fast(&newrighthalf->fusion_internal_tree);
        }
        if(key_node_par != NULL)
        // cout << "Key Node Par " << key_node_par->id << endl;
        // cout << "Removing node " << key_node->id << endl;
        allocator.free(key_node);
        key_node = key_node_par;
        if(key_node == NULL) { // went "above root," then we want to create new root
            // cout << "DSFSDFS" << endl;
            root = new_empty_node(allocator);
            key_node = root;
        }
        pmedian = newmedian;
        plefthalf = newlefthalf;
        prighthalf = newrighthalf;
        // cout << "printing new left half (" << newlefthalf->id << ")" << endl;
        // print_keys_sig_bits(&newlefthalf->fusion_internal_tree);
        // cout << "printing new right half (" << newrighthalf->id << ")" << endl;
        // print_keys_sig_bits(&newrighthalf->fusion_internal_tree);
        // cout << "New median: " << endl;
        // print_binary_uint64_big_endian(pmedian[7], true, 64, 8);
    }
    // printTree(root);
	// cout << ((int)plefthalf->fusion_internal_tree.tree.meta.size) << " " << ((int)prighthalf->fusion_internal_tree.tree.meta.size) << endl;
    // print_keys_sig_bits(&key_node->fusion_internal_tree);
    insert_key_node(&key_node->fusion_internal_tree, pmedian);
    if(!key_node->fusion_internal_tree.tree.meta.fast) {make_fast(&key_node->fusion_internal_tree);}
    // cout << "New things" << endl;
    // print_binary_uint64(key_node->fusion_internal_tree.tree.bitextract[0], true);
    // print_keys_sig_bits(&key_node->fusion_internal_tree);
    fusion_node tmp = {0};
    for(int i=0; i < key_node->fusion_internal_tree.tree.meta.size; i++) {
        insert(&tmp, key_node->fusion_internal_tree.keys[i]);
    }
    // cout << "dafdftmp " << extract_bits(&tmp.tree, tmp.keys[0]) << " " << extract_bits(&tmp.tree, tmp.keys[1])<< endl;
    // print_binary_uint64(tmp.tree.bitextract[0], true);
    // print_keys_sig_bits(&tmp);
    // cout << "dafdf " << extract_bits(&key_node->fusion_internal_tree.tree, key_node->fusion_internal_tree.keys[0]) << " " << extract_bits(&key_node->fusion_internal_tree.tree, key_node->fusion_internal_tree.keys[1])<< endl;
    // print_vec(key_node->fusion_internal_tree.tree.treebits, true, 16);
    int npos = ~query_branch_node(&key_node->fusion_internal_tree, pmedian);
    // cout << "npos is " << (~npos) << endl;
    //shifting to make room for new children (& we want to ignore the actual child that will be replaced by the two children, which should be at the position of the new key)
    //The child we want to replace is exactly at the position of the added median, since it was to the "right" of the key smaller than the median and to the "left" of the key larger than the median, so its position is one more than the position of the key to the "left" of the median now, so the position of the median. Thus we want to ignore that key and move the things to the right of that by one
    //cout << "Npos is " << npos << endl;
    //cout << "Key_node id is " << key_node->id << endl;
    for(int i=key_node->fusion_internal_tree.tree.meta.size /*ok that is ridiculous, fix that*/; i >= npos+2; i--) {
        key_node->children[i] = key_node->children[i-1];
    }
    // if(plefthalf->id == 6) cout << "DFSDF " << npos <<  endl;
    // printTree(root);
    key_node->children[npos] = plefthalf;
    key_node->children[npos+1] = prighthalf;
    plefthalf->parent = key_node;
    prighthalf->parent = key_node;
    // printTree(root);
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
    int branch = query_branch_node(&root->fusion_internal_tree, key);
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
    int branch = query_branch_node(&root->fusion_internal_tree, key);
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

size_t totalDepth(fusion_b_node* root, size_t dep) {
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



//assumes has full control (that is, node and par has the mutex). Also assumes the node is full and par is not full for simplicity
static void split_node(fusion_b_node* node, fusion_b_node* par) {
    // cout << "splitting node" << endl;
    // printTree(node);
    fusion_node* key_fnode = &node->fusion_internal_tree;

    fusion_b_node* newlefthalf = new fusion_b_node();
    // newlefthalf->id = ++IDCounter;
    // newlefthalf->id = ++IDCounter; //not thread safe but this is just for debugging anyways
    fusion_b_node* newrighthalf = new fusion_b_node();
    // newrighthalf->id = ++IDCounter;
    // newrighthalf->id = ++IDCounter; //not thread safe but this is just for debugging anyways

    constexpr int medpos = MAX_FUSION_SIZE/2; //two choices since max size even: maybe randomize?
    // cout << "SDFSDFSDF" << endl;
    for(int i = 0; i < medpos; i++) {
        insert(&newlefthalf->fusion_internal_tree, get_key_from_sorted_pos(&node->fusion_internal_tree, i));
        newlefthalf->children[i] = node->children[i];
        // if (node->children[i] != NULL)
        //     node->children[i]->parent = newlefthalf;
    }
    // cout << "SDFSDFSDF" << endl;
    newlefthalf->children[medpos] = node->children[medpos];
    if (node->children[medpos] != NULL)
        node->children[medpos]->parent = newlefthalf;
    for(int i = medpos+1; i < MAX_FUSION_SIZE; i++) {
        insert(&newrighthalf->fusion_internal_tree, get_key_from_sorted_pos(&node->fusion_internal_tree, i));
        newrighthalf->children[i-medpos-1] = node->children[i];
        // if (node->children[i] != NULL)
        //     node->children[i]->parent = newrighthalf;
    }
    newrighthalf->children[MAX_FUSION_SIZE-medpos-1] = node->children[MAX_FUSION_SIZE];
    // if (node->children[MAX_FUSION_SIZE] != NULL)
    //     node->children[MAX_FUSION_SIZE]->parent = newrighthalf;

    if(node->fusion_internal_tree.tree.meta.fast) {
        make_fast(&newlefthalf->fusion_internal_tree);
        make_fast(&newrighthalf->fusion_internal_tree);
    }

    __m512i median = get_key_from_sorted_pos(&node->fusion_internal_tree, medpos);
    // cout << "SDFSDFSDF" << endl;

    if(par == NULL) {
        // cout << "DSFSDFSDFSDFSDF" << endl;
        memset(&node->fusion_internal_tree, 0, sizeof(fusion_node) + sizeof(fusion_b_node*)*(MAX_FUSION_SIZE+1));
        insert_key_node(&node->fusion_internal_tree, median);
        // int pos = ~query_branch_node(&node->fusion_internal_tree, median);
        // for(int i=node->fusion_internal_tree.tree.meta.size /*ok that is ridiculous, fix that*/; i >= pos+2; i--) {
        //     node->children[i] = node->children[i-1];
        // }
        node->children[0] = newlefthalf;
        node->children[1] = newrighthalf;
        // newlefthalf->parent = node;
        // newrighthalf->parent = node;
        // cout << "node is root" << endl;
        // printTree(node);
        return;
    }

    // cout << "Went up to par" << endl;

    node->deleted = true;
    insert_key_node(&par->fusion_internal_tree, median);
    int pos = ~query_branch_node(&par->fusion_internal_tree, median);
    for(int i=par->fusion_internal_tree.tree.meta.size /*ok that is ridiculous, fix that*/; i >= pos+2; i--) {
        par->children[i] = par->children[i-1];
    }
    par->children[pos] = newlefthalf;
    par->children[pos+1] = newrighthalf;
    // newlefthalf->parent = par;
    // newrighthalf->parent = par;
}

void parallel_insert_full_tree(fusion_b_node* root, __m512i key) {
    //Here we really just don't want the root to be null, cause multiple threads doing stuff and all that
    assert(root != NULL);

    // printTree(root);

    fusion_b_node* par = NULL;
    fusion_b_node* cur = root;
    //REALLY need to do upgradable mutexes, cause this is unweildy, although even upgradable mutexes doesn't really solve all of our problems, but it does solve some.
    //NEW PLAN: "root" is actually a dummy root-->>it stores absolutely nothing except a mutex, so that we ALWAYS have two mutexes active.
    //When want to "upgrade" our mutexes, do it as so: unlock_shared the child, then lock the child, then unlock_shared the parent and lock the parent. NVM THIS IS DEADLOCK
    //Instead: have a third parameter, which is the pointer of the parent of the thing that we want to EXCLUSIVELY LOCK. Then we, upon having issues (if child node, we claim lock, and then if after locked its full we do as usual and just rerun)
    //rerun the function the function specifying the third parameter where to lock.
    (cur->mtx).lock_shared();

    if(node_full(&cur->fusion_internal_tree)) {
        if((cur->mtx).try_upgrade()) {
            split_node(cur, par);
            (cur->mtx).unlock();
        }
        else {
            (cur->mtx).unlock_shared();
        }
        return parallel_insert_full_tree(root, key);
    }
    int branch = query_branch_node(&cur->fusion_internal_tree, key);
    if(branch < 0) { //say exact match just return & do nothing
        (cur->mtx).unlock_shared();
        return;
    }
    if(cur->children[branch] == NULL) {
        if((cur->mtx).try_upgrade()) {
            if(node_full(&cur->fusion_internal_tree)) {
                split_node(cur, par);
                (cur->mtx).unlock();
                return parallel_insert_full_tree(root, key);
            }
            insert_key_node(&cur->fusion_internal_tree, key);
            (cur->mtx).unlock();
            return;
        }
        (cur->mtx).unlock_shared();
        return parallel_insert_full_tree(root, key);
    }
    par = cur;
    cur = cur->children[branch];
    (cur->mtx).lock_shared();
    // cout << "LOCK NODE: " << lock_node << endl;
    while(true) { //assumes par and cur exist and are locked according to the paradigm.
        if(node_full(&cur->fusion_internal_tree)) {
            if(!(par->mtx).try_upgrade()) {
                (cur->mtx).unlock_shared();
                return parallel_insert_full_tree(root, key);
            }
            (cur->mtx).upgrade();
            split_node(cur, par);
            (par->mtx).unlock();
            (cur->mtx).unlock();
            return parallel_insert_full_tree(root, key);
        }
        int branch = query_branch_node(&cur->fusion_internal_tree, key);
        // cout << "branch, " << branch << endl;
        // cout << (cur->children[branch] == cur) << endl;
        if(branch < 0) { //say exact match just return & do nothing
            (par->mtx).unlock_shared();
            (cur->mtx).unlock_shared();
            return;
        }
        if(cur->children[branch] == NULL) {
            (cur->mtx).upgrade();
            insert_key_node(&cur->fusion_internal_tree, key);
            (par->mtx).unlock_shared();
            (cur->mtx).unlock();
            return;
        }
        fusion_b_node* nchild = cur->children[branch];
        (nchild->mtx).lock_shared();
        (par->mtx).unlock_shared();
        par = cur;
        cur = nchild;
    }
}

// void parallel_insert_full_tree(fusion_b_node* root, __m512i key, fusion_b_node* lock_node /* = NULL*/) {
//     //Here we really just don't want the root to be null, cause multiple threads doing stuff and all that
//     assert(root != NULL);

//     // printTree(root);

//     fusion_b_node* par = NULL;
//     fusion_b_node* cur = root;
//     //REALLY need to do upgradable mutexes, cause this is unweildy, although even upgradable mutexes doesn't really solve all of our problems, but it does solve some.
//     //NEW PLAN: "root" is actually a dummy root-->>it stores absolutely nothing except a mutex, so that we ALWAYS have two mutexes active.
//     //When want to "upgrade" our mutexes, do it as so: unlock_shared the child, then lock the child, then unlock_shared the parent and lock the parent. NVM THIS IS DEADLOCK
//     //Instead: have a third parameter, which is the pointer of the parent of the thing that we want to EXCLUSIVELY LOCK. Then we, upon having issues (if child node, we claim lock, and then if after locked its full we do as usual and just rerun)
//     //rerun the function the function specifying the third parameter where to lock.
//     if(lock_node == cur) {
//         // cout << "NANI THE F" << endl;
//         // (cur->mtx).unlock_shared();
//         (cur->mtx).lock();
//         // cout << "NANI THE F2" << endl;
//     }
//     else {
//         (cur->mtx).lock_shared();
//     }
//     if(node_full(&cur->fusion_internal_tree)) {
//         if(lock_node != cur) {
//             (cur->mtx).unlock_shared();
//             (cur->mtx).lock();
//         }
//         //note that the root NEVER gets deleted, so we just check if it was split
//         if(node_full(&cur->fusion_internal_tree)) {
//             split_node(cur, par);
//         }
//         (cur->mtx).unlock();
//         return parallel_insert_full_tree(root, key);
//     }
//     int branch = query_branch_node(&cur->fusion_internal_tree, key);
//     if(branch < 0) { //say exact match just return & do nothing
//         if(lock_node == cur) {
//             (cur->mtx).unlock();
//         }
//         else {
//             (cur->mtx).unlock_shared();
//         }
//         return;
//     }
//     if(cur->children[branch] == NULL) {
//         if(lock_node != cur) {
//             (cur->mtx).unlock_shared();
//             (cur->mtx).lock();
//         }
//         if(cur->deleted || cur->children[branch] != NULL) {
//             (cur->mtx).unlock();
//             return parallel_insert_full_tree(root, key);
//         }
//         if(node_full(&cur->fusion_internal_tree)) {
//             split_node(cur, par);
//             (cur->mtx).unlock();
//             return parallel_insert_full_tree(root, key);
//         }
//         insert_key_node(&cur->fusion_internal_tree, key);
//         (cur->mtx).unlock();
//         return;
//     }
//     par = cur;
//     cur = cur->children[branch];
//     if(lock_node == par || lock_node == cur) {
//         (cur->mtx).lock();
//     }
//     else {
//         (cur->mtx).lock_shared();
//     }
//     // cout << "LOCK NODE: " << lock_node << endl;
//     while(true) { //assumes par and cur exist and are locked according to the paradigm.
//         if(par == lock_node) {
//             // cout << "FDSFDS" << endl;
//             if(node_full(&cur->fusion_internal_tree)) {
//                 split_node(cur, par);
//             }
//             (par->mtx).unlock();
//             (cur->mtx).unlock();
//             return parallel_insert_full_tree(root, key);
//         }
//         if(node_full(&cur->fusion_internal_tree)) {
//             if(cur == lock_node) {
//                 (cur->mtx).unlock();
//             }
//             else {
//                 (cur->mtx).unlock_shared();
//             }
//             (par->mtx).unlock_shared();
//             return parallel_insert_full_tree(root, key, par);
//         }
//         int branch = query_branch_node(&cur->fusion_internal_tree, key);
//         // cout << "branch, " << branch << endl;
//         // cout << (cur->children[branch] == cur) << endl;
//         if(branch < 0) { //say exact match just return & do nothing
//             (par->mtx).unlock_shared();
//             if(cur == lock_node)
//                 (cur->mtx).unlock();
//             else
//                 (cur->mtx).unlock_shared();
//             return;
//         }
//         if(cur->children[branch] == NULL) {
//             // cout << "fdafasdfas " << (cur == lock_node) << endl;
//             (cur->mtx).unlock_shared();
//             // cout << "NANDE???" << endl;
//             (cur->mtx).lock();
//             // cout <<" DFSDFSD " << endl;
//             if(cur->deleted || cur->children[branch] != NULL) {
//                 (par->mtx).unlock_shared();
//                 (cur->mtx).unlock();
//                 return parallel_insert_full_tree(root, key);
//             }
//             if(node_full(&cur->fusion_internal_tree)) {
//                 (cur->mtx).unlock();
//                 (par->mtx).unlock_shared();
//                 return parallel_insert_full_tree(root, key, par);
//             }
//             insert_key_node(&cur->fusion_internal_tree, key);
//             (par->mtx).unlock_shared();
//             (cur->mtx).unlock();
//             return;
//         }
//         fusion_b_node* nchild = cur->children[branch];
//         // cout << cur << " " << cur->children[branch] << endl;
//         // cout << (nchild == cur) << endl;
//         if(cur == lock_node || nchild == lock_node) {
//             (nchild->mtx).lock();
//         }
//         else {
//             (nchild->mtx).lock_shared();
//         }
//         (par->mtx).unlock_shared();
//         par = cur;
//         cur = nchild;
//         //Actually since we have three nodes locked at once, it should be possible to not have to like rerun the function and stuff, way its done rn is p bad
//         //But whatever as a first iteration hopefully it works
//         //Actually maybe that doesn't work anyways
//     }

    // // cout << "FDFS" << endl;
    // (root->mtx).lock_shared();
    // // cout << "FDFS2" << endl;
    // if(root->deleted) {
    //     return parallel_insert_full_tree(root->parent, key);
    // }
    // //The following seems rather sus
    // if(node_full(&root->fusion_internal_tree)) {
    //     // cout << "DFSDFSDFD" << endl;
    //     (root->mtx).unlock_shared();
    //     (root->mtx).lock();
    //     fusion_b_node* par = root->parent;
    //     if(!node_full(&root->fusion_internal_tree)) {
    //         (root->mtx).unlock();
    //         return parallel_insert_full_tree(root, key);
    //     }
    //     //Locking the parent first ENSURES that, when we lock the root, no nodes are gonna be waiting to get the root
    //     //EDIT: not really lol but whatever. For now we just ignore this. Maybe add garbage collection, maybe do something less dumb Idk
    //     if(par != NULL) {
    //         (par->mtx).lock();
    //         if(node_full(&par->fusion_internal_tree)) {
    //             (root->mtx).unlock();
    //             (par->mtx).unlock();
    //             return parallel_insert_full_tree(par, key);
    //         }
    //     }
    //     split_node(root, par);
    //     (root->mtx).unlock();
    //     if(par != NULL) {
    //         (par->mtx).unlock();
    //     }
    //     return parallel_insert_full_tree(par == NULL? root : par, key);
    // }

    // int branch = query_branch_node(&root->fusion_internal_tree, key);
    // if(branch < 0) { //say exact match just return & do nothing
    //     (root->mtx).unlock_shared();
    //     return;
    // }
    // if(root->children[branch] == NULL) {
    //     // cout << "FDDCER" << endl;
    //     (root->mtx).unlock_shared();
    //     // cout << "FFDFERR45" << endl;
    //     (root->mtx).lock();
    //     // cout << "FFDFERR43335" << endl;
    //     if(node_full(&root->fusion_internal_tree) || root->deleted) {
    //         // cout << "FDDCER2" << endl;
    //         (root->mtx).unlock();
    //         return parallel_insert_full_tree(root, key);
    //     }
    //     // cout << "FDDCER3" << endl;
    //     insert_key_node(&root->fusion_internal_tree, key);
    //     (root->mtx).unlock();
    //     return;
    // }
    // fusion_b_node* child = root->children[branch];
    // (root->mtx).unlock_shared();
    // return parallel_insert_full_tree(child, key);
// }

// __m512i* parallel_successor(fusion_b_node* root, __m512i key, bool foundkey /*=false*/, bool needbig/*=false*/) { //returns null if there is no successor
// 	if(root == NULL) return NULL;
//     (root->mtx).shared_lock();
//     if(root->deleted) {
//         return parallel_successor(root->parent, key, ) // ????????
//     }
// 	if(foundkey) {
// 		//cout << "Found key" << endl;
// 		int branch = needbig ? root->fusion_internal_tree.tree.meta.size : 0;
// 		__m512i* ans = successor(root->children[branch], key, true, needbig);
// 		branch = needbig ? (root->fusion_internal_tree.tree.meta.size-1) : 0;
// 		return ans == NULL ? &root->fusion_internal_tree.keys[get_real_pos_from_sorted_pos(&root->fusion_internal_tree, branch)] : ans;
// 	}
//     int branch = query_branch_node(&root->fusion_internal_tree, key);
//     //print_keys_sig_bits(&root->fusion_internal_tree);
//     //cout << "Branch is " << branch << ", and is it null: " << (root->children[branch < 0 ? 0 : branch] == NULL) << endl;
//     if(branch < 0) { // exact key match found
//         branch = ~branch;
//         if(root->children[branch+1] != NULL) { //This was root->children[branch] before, but that caused no problems for some reason? wtf? Oh, maybe cause its a B-tree that just never really happens. Yeah I think if there's just one child then that's enough
// 	        return successor(root->children[branch+1], key, true, false);
// 	    }
// 	    else if(branch+1 < root->fusion_internal_tree.tree.meta.size) {
// 	    	return &root->fusion_internal_tree.keys[get_real_pos_from_sorted_pos(&root->fusion_internal_tree, branch+1)];
// 	   	}
// 	    return NULL;
//     }
//     __m512i* ans;
//     if(root->children[branch] == NULL || (ans = successor(root->children[branch], key)) == NULL){ //when didn't find the successor below, we now look if the successor is here
//     	if(branch < root->fusion_internal_tree.tree.meta.size) {
//     		return &root->fusion_internal_tree.keys[get_real_pos_from_sorted_pos(&root->fusion_internal_tree, branch)];
//     	}
//     	else return NULL;
//     }
//     return ans;
// }