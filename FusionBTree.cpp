#include "FusionBTree.h"

fusion_b_node* new_empty_node() {
    return (fusion_b_node*) calloc(1, sizeof(fusion_b_node));
}

fusion_b_node* search_key_full_tree(fusion_b_node* root, __m512i key) {
    int branch = query_branch(&root->fusion_internal_tree, key);
    if(branch < 0 || root->children[branch] == NULL) { // either key exact match found or nowhere down to go
        branch = ~branch;
        return root;
    }
    return search_key_full_tree(root->children[branch], key);
}

fusion_b_node* insert_full_tree(fusion_b_node* root, __m512i key) {
    if(root == NULL) {
        root = new_empty_node();
        insert(&root->fusion_internal_tree, key);
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
    while(node_full(&key_node->fusion_internal_tree)) {
        fusion_node* key_fnode = &key_node->fusion_internal_tree;
        int keypos = query_branch(key_fnode, pmedian);
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
                insert(&newlefthalf->fusion_internal_tree, curkey);
                newlefthalf->children[i] = curchild;
                curchild->parent = newlefthalf;
            }
            else if(i == MAX_FUSION_SIZE/2) {
                newmedian = curkey;
                newlefthalf->children[i] = curchild;
                curchild->parent = newlefthalf;
            }
            else {
                insert(&newlefthalf->fusion_internal_tree, curkey);
                newrighthalf->children[i-MAX_FUSION_SIZE/2-1] = curchild;
                curchild->parent = newrighthalf;
            }
        }
        { //one extra branch than keys, so add this child
            fusion_b_node* curchild = (MAX_FUSION_SIZE+1) == (keypos+1) ? prighthalf : key_node->children[MAX_FUSION_SIZE+1];
            newrighthalf->children[MAX_FUSION_SIZE/2] = curchild;
            curchild->parent = newrighthalf;
        }
        fusion_b_node* key_node_par = key_node->parent;
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

    insert(&key_node->fusion_internal_tree, pmedian);
    int npos = ~query_branch(&key_node->fusion_internal_tree, pmedian);
    //shifting to make room for new children (& we want to ignore the actual child that will be replaced by the two children, which should be at the position of the new key)
    //The child we want to replace is exactly at the position of the added median, since it was to the "right" of the key smaller than the median and to the "left" of the key larger than the median, so its position is one more than the position of the key to the "left" of the median now, so the position of the median. Thus we want to ignore that key and move the things to the right of that by one
    for(int i=key_node->fusion_internal_tree.tree.meta.size /*ok that is ridiculous, fix that*/; i >= npos+2; i--) {
        key_node->children[i] = key_node->children[i-1];
    }
    key_node->children[npos] = plefthalf;
    key_node->children[npos+1] = prighthalf;
    
    return root;
}