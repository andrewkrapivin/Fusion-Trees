#include "FusionBTree.h"
#include "HelperFuncs.h"
#include <iostream>
#include <cstring>

namespace FusionTree {

    uint64_t IDCounter = 0;

    fusion_b_node* new_empty_node() {
        fusion_b_node* new_node = static_cast<fusion_b_node*>(std::aligned_alloc(64, sizeof(fusion_b_node)));
        memset(new_node, 0, sizeof(fusion_b_node));
        new_node->id = IDCounter++;
        return new_node;
    }

    fusion_b_node* search_key_full_tree(fusion_b_node* root, __m512i key) {
        int branch = root->internal_mini_tree.query_branch(key);
        if(branch < 0 || root->children[branch] == NULL) { // either key exact match found or nowhere down to go
            branch = ~branch;
            return root;
        }
        return search_key_full_tree(root->children[branch], key);
    }

    fusion_b_node* insert_full_tree(fusion_b_node* root, __m512i key) {
        if(root == NULL) {
            root = new_empty_node();
            root->internal_mini_tree.insert(key);
            return root;
        }

        fusion_b_node* key_node = search_key_full_tree(root, key);
        if(!key_node->internal_mini_tree.node_full()) {
            key_node->internal_mini_tree.insert(key);
            return root;
        }
        __m512i pmedian = key;
        fusion_b_node* plefthalf = NULL;
        fusion_b_node* prighthalf = NULL;
        while(key_node->internal_mini_tree.node_full()) {
            FastInsertMiniTree* key_fnode = &key_node->internal_mini_tree;
            int keypos = key_fnode->query_branch(pmedian);
            if(keypos < 0) //already in the tree. Should only be true on the first loop time. If something breaks and that isnt the case, this will be bad
                return root;
            __m512i newmedian;
            fusion_b_node* newlefthalf = new_empty_node();
            fusion_b_node* newrighthalf = new_empty_node();
            //just adding half the keys to the left size and half to the right
            for(int i=0, j=0; i < MAX_FUSION_SIZE+1; i++) {
                fusion_b_node* curchild = i == keypos ? plefthalf : (i == (keypos+1) ? prighthalf : key_node->children[j]); //p hacky, probably fix this
                __m512i curkey = i == keypos ? pmedian : key_fnode->get_key_from_sorted_pos(j++);// and this
                if (i < MAX_FUSION_SIZE/2) {
                    newlefthalf->internal_mini_tree.insert(curkey);
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
                    newrighthalf->internal_mini_tree.insert(curkey);
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
        key_node->internal_mini_tree.insert(pmedian);
        int npos = ~key_node->internal_mini_tree.query_branch(pmedian);
        //shifting to make room for new children (& we want to ignore the actual child that will be replaced by the two children, which should be at the position of the new key)
        //The child we want to replace is exactly at the position of the added median, since it was to the "right" of the key smaller than the median and to the "left" of the key larger than the median, so its position is one more than the position of the key to the "left" of the median now, so the position of the median. Thus we want to ignore that key and move the things to the right of that by one
        for(int i=key_node->internal_mini_tree.meta.size /*ok that is ridiculous, fix that*/; i >= npos+2; i--) {
            key_node->children[i] = key_node->children[i-1];
        }
        key_node->children[npos] = plefthalf;
        key_node->children[npos+1] = prighthalf;
        plefthalf->parent = key_node;
        prighthalf->parent = key_node;
        
        return root;
    }

    __m512i* successor(fusion_b_node* root, __m512i key, bool foundkey /*=false*/, bool needbig/*=false*/) { //returns null if there is no successor
        if(root == NULL) return NULL;
        if(foundkey) {
            int branch = needbig ? root->internal_mini_tree.meta.size : 0;
            __m512i* ans = successor(root->children[branch], key, true, needbig);
            branch = needbig ? (root->internal_mini_tree.meta.size-1) : 0;
            return ans == NULL ? &root->internal_mini_tree.keys[root->internal_mini_tree.get_real_pos_from_sorted_pos(branch)] : ans;
        }
        int branch = root->internal_mini_tree.query_branch(key);
        if(branch < 0) { // exact key match found
            branch = ~branch;
            if(root->children[branch+1] != NULL) { //This was root->children[branch] before, but that caused no problems for some reason? wtf? Oh, maybe cause its a B-tree that just never really happens. Yeah I think if there's just one child then that's enough
                return successor(root->children[branch+1], key, true, false);
            }
            else if(branch+1 < root->internal_mini_tree.meta.size) {
                return &root->internal_mini_tree.keys[root->internal_mini_tree.get_real_pos_from_sorted_pos(branch+1)];
            }
            return NULL;
        }
        __m512i* ans;
        if(root->children[branch] == NULL || (ans = successor(root->children[branch], key)) == NULL){ //when didn't find the successor below, we now look if the successor is here
            if(branch < root->internal_mini_tree.meta.size) {
                return &root->internal_mini_tree.keys[root->internal_mini_tree.get_real_pos_from_sorted_pos(branch)];
            }
            else return NULL;
        }
        return ans;
    }

    __m512i* predecessor(fusion_b_node* root, __m512i key, bool foundkey /*=false*/, bool needbig/*=false*/) { //returns null if there is no successor
        if(root == NULL) return NULL;
        if(foundkey) {
            int branch = needbig ? root->internal_mini_tree.meta.size : 0;
            __m512i* ans = predecessor(root->children[branch], key, true, needbig);
            branch = needbig ? (root->internal_mini_tree.meta.size-1) : 0;
            return ans == NULL ? &root->internal_mini_tree.keys[root->internal_mini_tree.get_real_pos_from_sorted_pos(branch)] : ans;
        }
        int branch = root->internal_mini_tree.query_branch(key);
        if(branch < 0) { // exact key match found
            branch = ~branch;
            if(root->children[branch] != NULL) {
                return predecessor(root->children[branch], key, true, true);
            }
            else if(branch-1 >= 0) {
                return &root->internal_mini_tree.keys[root->internal_mini_tree.get_real_pos_from_sorted_pos(branch-1)];
            }
            return NULL;
        }
        __m512i* ans;
        if(root->children[branch] == NULL || (ans = predecessor(root->children[branch], key)) == NULL){ //when didn't find the successor below, we now look if the successor is here
            if(branch-1 >= 0) {
                return &root->internal_mini_tree.keys[root->internal_mini_tree.get_real_pos_from_sorted_pos(branch-1)];
            }
            else return NULL;
        }
        return ans;
    }

    void printTree(fusion_b_node* root, int indent) {
        if(root->visited) {
            for(int i = 0; i < indent; i++) std::cout << " ";
            std::cout << "Strange. Already visited node " << root->id << std::endl;
            return;
        }
        root->visited = true;
        int branchcount = 0;
        for(int i = 0; i < MAX_FUSION_SIZE+1; i++) {
            if(root->children[i] != NULL)
                branchcount++;
        }
        for(int i = 0; i < indent; i++) std::cout << " ";
        if(root->parent != NULL)
            std::cout << "Node " << root->id << " has " << root->parent->id << " as a parent." << std::endl;
        for(int i = 0; i < indent; i++) std::cout << " ";
        std::cout << "Node " << root->id << " has " << branchcount << " children. Exploring them now: {" << std::endl;
        for(int i = 0; i < MAX_FUSION_SIZE+1; i++) {
            if(root->children[i] != NULL)
                printTree(root->children[i], indent+4);
        }
        root->visited = false;
        for(int i = 0; i < indent; i++) std::cout << " ";
        std::cout << "}" << std::endl;
    }

    int maxDepth(fusion_b_node* root) {
        int ans = 0;
        for(int i = 0; i < MAX_FUSION_SIZE+1; i++) {
            if(root->children[i] != NULL)
                ans = std::max(maxDepth(root->children[i])+1, ans);
        }
        return ans;
    }
}