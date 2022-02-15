#ifndef FUSION_B_TREE_H_INCLUDED
#define FUSION_B_TREE_H_INCLUDED

#include "fusion_tree.h"

//maybe do like numbranches macro defined as max_fusion_size+1 to make things a bit nicer?

namespace FusionTree {
    typedef struct fusion_b_node{
        FastInsertMiniTree internal_mini_tree;
        fusion_b_node* children[MAX_FUSION_SIZE+1];
        fusion_b_node* parent;
        //temporary debug vals
        bool visited;
        uint64_t id;
    } fusion_b_node;

    fusion_b_node* new_empty_node();
    fusion_b_node* search_key_full_tree(fusion_b_node* root, __m512i key); //searches key in the full tree. Yeah naming needs to be improved. Also what should this actually return? Cause returning fusion_b_node doesn't give you direct say way to get successor or anything
    fusion_b_node* insert_full_tree(fusion_b_node* root, __m512i key);
    __m512i* successor(fusion_b_node* root, __m512i key, bool foundkey=false, bool needbig=false);
    __m512i* predecessor(fusion_b_node* root, __m512i key, bool foundkey=false, bool needbig=false);
    void printTree(fusion_b_node* root, int indent=0);
    int maxDepth(fusion_b_node* root);
}

#endif
