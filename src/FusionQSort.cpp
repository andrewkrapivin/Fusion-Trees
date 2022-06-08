#include "FusionQSort.h"
#include <algorithm>
#include <iostream>
#include <cstring>
#include "HelperFuncs.h"

//for now this sort explicitly assumes all keys are different and will not work otherwise. Ok it probably will work otherwise actually, but just be safe
void FusionQSort(__m512i arr[], size_t length) {

    if(length < 200) {
        // std::cout << "sorting using std::sort" << endl;
        std::sort(arr, arr+length, compare__m512i);
        return;
    }

    std::sort(arr, arr+MAX_FUSION_SIZE*5, compare__m512i);
    
    for(size_t i = 2, j=0; i < MAX_FUSION_SIZE*5; i+=5, j++) {
        swap(arr[i], arr[j]);
    }

    // std::cout << "Length: " << length << std::endl;

    //Being a bit lazy here, but it should not be the majority of the time so whatever
    //Obviously optimize the make fast & stuff to easily make static node, and also randomize which keys are chosen?
    fusion_node* pivot_node = new_empty_fusion_node();
    for(size_t i=0; i < MAX_FUSION_SIZE; i++) {
        insert(pivot_node, arr[i]);
    }
    make_fast(pivot_node);
    // print_keys_sig_bits(pivot_node);

    //Again, probably branching factor of 8 best for this application, but idk. Too lazy to do that for now.
    __m512i* tmparr1 = static_cast<__m512i*>(std::aligned_alloc(64, sizeof(__m512i)*length));
    uint16_t* tmparr2 = (uint16_t*)std::malloc(length*2);
    /*for(size_t i=MAX_FUSION_SIZE; i < length; i++) {
        tmparr2[i] = extract_bits(&pivot_node->tree, arr[i]);
    }

    //Really this should be parallelized, but whatever for now
    size_t count_each_pivot[MAX_FUSION_SIZE+1] = {0};
    size_t count_each_pivot2[MAX_FUSION_SIZE+1] = {0};
    for(size_t i=MAX_FUSION_SIZE; i < length; i++){
        uint16_t pos_tree = search_pos_tree_fast(pivot_node, tmparr2[i]);
        count_each_pivot[pos_tree+1]++;
        tmparr2[i] = pos_tree;
    }

    for(size_t i=1; i<MAX_FUSION_SIZE; i++) {
        count_each_pivot[i] += count_each_pivot[i-1];
        count_each_pivot2[i] = count_each_pivot[i];
    }

    for(size_t i = 0; i < length; i++) {
        tmparr1[count_each_pivot[tmparr2[i]]++] = arr[i];
    }*/

    size_t count_each_pivot[MAX_FUSION_SIZE+2] = {0};
    size_t count_each_pivot2[MAX_FUSION_SIZE+2] = {0};
    size_t count_each_pivot3[MAX_FUSION_SIZE+2] = {0};
    for(size_t i=MAX_FUSION_SIZE; i < length; i++){
        uint16_t pos_tree = query_branch_fast(pivot_node, arr[i]);
        // cout << "Pos_tree: " << pos_tree << endl;
        // print_binary_uint64_big_endian(arr[i][7], true, 64, 16);
        count_each_pivot[pos_tree+1]++;
        tmparr2[i] = pos_tree;
    }

    count_each_pivot2[0] = 0;
    for(size_t i=1; i<MAX_FUSION_SIZE+2; i++) {
        // std::cout << "Pivot "<< i << " has " << count_each_pivot[i] << std::endl;
        count_each_pivot3[i-1] = count_each_pivot[i];
        count_each_pivot[i] ++;
        count_each_pivot[i] += count_each_pivot[i-1];
        count_each_pivot2[i] = count_each_pivot[i];
        // std::cout << "New Pivot "<< i << " has " << count_each_pivot[i] << std::endl;
    }

    for(size_t i = MAX_FUSION_SIZE; i < length; i++) {
        std::memcpy(&tmparr1[count_each_pivot[tmparr2[i]]++], &arr[i], 64);
        // print_binary_uint64_big_endian(arr[i][7], true, 64, 16);
        // std::cout << "Added key at " << tmparr2[i] << " to " << (count_each_pivot[tmparr2[i]]-1) << std::endl;
    }
    for(size_t i = 0; i < MAX_FUSION_SIZE; i++) {
        // for(size_t j = 0; j < count_each_pivot[i]; j ++) {
        //     if(!compare__m512i(tmparr1[j], pivot_node->keys[i])) {
        //         cout << "Weird stuff at " << i << " and " << j << endl;
        //         print_binary_uint64_big_endian(pivot_node->keys[i][7], true, 64, 16);
        //         print_binary_uint64_big_endian(tmparr1[j][7], true, 64, 16);
        //         exit(1);
        //     }
        // }
        std::memcpy(&tmparr1[count_each_pivot[i]], &pivot_node->keys[i], 64);
        // std::cout << "Added pivot at " << i << " to " << (count_each_pivot[i]) << std::endl;
    }

    for(size_t i = 0; i < length; i++) {
        std::memcpy(&arr[i], &tmparr1[i], 64);
        // std::cout << i << std::endl;
        // print_binary_uint64_big_endian(arr[i][7], true, 64, 16);
    }

    // count_each_pivot[MAX_FUSION_SIZE+1] = length-MAX_FUSION_SIZE;

    // for(size_t i=0, j=0, l=0; i < MAX_FUSION_SIZE+2; i++) {
    //     size_t k = j;
    //     for(l=0; j < count_each_pivot[i]+i; j++, l++) {
    //         arr[j] = tmparr1[j-i];
    //     }
    //     FusionQSort(&arr[j], l);
    //     if(j < length)
    //         arr[j++] = pivot_node->keys[i];
    // }

    free(pivot_node);
    free(tmparr1);
    free(tmparr2);

    for (size_t i = 0; i < MAX_FUSION_SIZE+1; i++) {
        // cout << "Recursively sorting part " << i << " starting at " << count_each_pivot2[i] << " with length " << count_each_pivot3[i] << endl;
        FusionQSort(&arr[count_each_pivot2[i]], count_each_pivot3[i]);
        // std::sort(&arr[count_each_pivot2[i]], &arr[count_each_pivot2[i]]+count_each_pivot3[i], compare__m512i);
    }

}