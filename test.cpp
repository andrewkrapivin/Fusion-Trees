#include "fusion_tree.h"
#include "HelperFuncs.h"
#include "FusionBTree.h"
#include <boost/container/set.hpp>

#include <random>
#include <chrono>
#include <iostream>
#include <climits>
#include <immintrin.h>
#include <cstdint>
#include <algorithm>
#include <cstring>
#include <set>
#include <cassert>

uint64_t gen_random_uint64(mt19937& generator) {
    uint64_t A;
    std::uniform_int_distribution<uint64_t> temporary_distribution(0, ULLONG_MAX);
    A = temporary_distribution(generator);
    return A;
}

__m512i gen_random_vec(mt19937& generator) {
    __m512i A;
    std::uniform_int_distribution<uint64_t> temporary_distribution(0, ULLONG_MAX);
    for(int i=0; i < 8; i++){
        A[i] = temporary_distribution(generator);
    }
    return A;
}

__m512i gen_random_vec_one_bit(mt19937& generator) {
    __m512i A;
    std::uniform_int_distribution<uint64_t> temporary_distribution1(0, 63);
    uint64_t posbyte = temporary_distribution1(generator);
    __mmask64 k = _cvtu64_mask64(1ull << posbyte);
    std::uniform_int_distribution<uint64_t> temporary_distribution2(0, 7);
    uint64_t posbit = temporary_distribution2(generator);
    A = _mm512_maskz_set1_epi8 (k, (1ull << posbit));
    /*cout << "Generated random bit at byte " << posbyte << " and bit " << posbit << endl;
    print_binary_uint64(_cvtmask64_u64(k), true);
    print_binary_uint64((1ull << posbit), true);
    print_vec(A, true, 8);*/
    return A;
}

vector<int> generate_random_positions(mt19937& generator, int count, int maxpos=511){
    set<int> tmp;
    std::uniform_int_distribution<uint64_t> temporary_distribution(0, maxpos);
    while(tmp.size() < count) {
        tmp.insert(temporary_distribution(generator));
    }
    vector<int> random_pos;
    for (std::set<int>::iterator it=tmp.begin(); it!=tmp.end(); ++it)
        random_pos.push_back(*it);
    shuffle(random_pos.begin(), random_pos.end(), generator);
    return random_pos;
}

__m512i gen_vec_one_bit(int bit_pos) {
    __m512i A;
    uint64_t posbyte = bit_pos/8;
    __mmask64 k = _cvtu64_mask64(1ull << posbyte);
    uint64_t posbit = bit_pos%8;
    A = _mm512_maskz_set1_epi8 (k, (1ull << posbit));
    /*cout << "Generated random bit at byte " << posbyte << " and bit " << posbit << endl;
    print_binary_uint64(_cvtmask64_u64(k), true);
    print_binary_uint64((1ull << posbit), true);
    print_vec(A, true, 8);*/
    return A;
}

__m512i gen_random_vec_all_but_one_bit(mt19937& generator) {
    return _mm512_andnot_si512(gen_random_vec_one_bit(generator), _mm512_set1_epi8(255));
}

void print_node_info(fusion_node& test_node) {
    cout << "Extraction mask:" << endl;
    print_binary_uint64(test_node.tree.byte_extract, true);
    print_binary_uint64(test_node.tree.bitextract[0], false, 8);
    cout << " ";
    print_binary_uint64(test_node.tree.bitextract[1], true, 8);
    cout << "Tree and ignore bits:" << endl;
    print_vec(test_node.tree.treebits, true, 16);
    print_vec(test_node.ignore_mask, true, 16);
    cout << "Sorted to real positions:" << endl;
    print_vec(test_node.key_positions, true, 8);
}

int main(){
    unsigned seed = chrono::steady_clock::now().time_since_epoch().count();
    //we made this work! 2767760278, 3339913857, 3110249540(4 tests), 3998269307 (size 8!), 4151455078 (size 8), 2969908123 (this was just because I didn't generate unique random vectors), 1262589155 (fixed mask_pos for tmp>=8, but also some weird behavior?)
    mt19937 generator (seed);

	__m512i A = {0, 1, 0, 0, 0, 0, 0, 3}; //we treat the number as little endian, cause otherwise it is inconsistent.
    //Cause the CPU stores each number as little endian, and for let's say the 65th digit to be one, we initialize it as in A, which is really weird to think about, because the earlier numbers are more significant as normal, but then within the numbers its the opposite, but, well, whatever
    //Little/big endian is pretty annoying
	__m512i B = {0, 0, 0, 0, 0, 0, 0, 0};
	
	//__m512i X = gen_random_vec(generator);
    //print_vec(B, true);
    /*cout << first_diff_bit_pos(A, B) << endl;
    cout << first_diff_bit_pos(B, X) << endl;*/

    /*print_binary_uint64(65, true);
    print_hex_uint64(65, true);
    uint64_t x = 65;
    
	cout << _lzcnt_u32(1) << endl;
	
	cout << first_diff_bit_pos(A, B) <<" afdaff" <<  endl;*/

    __m256i test = {0};

    //print_vec(setbit_each_epi16_in_range(test, 2, 0, 7, 1), true, 16);

    //testing stuff
    fusion_node test_node = {0};
    /*add_position_to_extraction_mask(&test_node.tree, first_diff_bit_pos(B, X));
    add_position_to_extraction_mask(&test_node.tree, first_diff_bit_pos(A, B));
    print_binary_uint64(test_node.tree.byte_extract, true);
    print_binary_uint64(test_node.tree.bitextract[0], true, 8);
    cout << extract_bits(&test_node.tree, A) << endl;
    cout << extract_bits(&test_node.tree, B) << endl;
    cout << extract_bits(&test_node.tree, X) << endl; //surprisingly enough seems right
    cout << diff_bit_to_mask_pos(&test_node, 511) << endl;
    cout << diff_bit_to_mask_pos(&test_node, 480) << endl;
    cout << diff_bit_to_mask_pos(&test_node, 420) << endl;*/
    //cout << insert(&test_node, X) << endl;

    //let's test extraction
    /*test_node.tree.byte_extract = (1ull << 8) + (1ull << 56) + 1ull;
    cout << test_node.tree.byte_extract << endl;

    test_node.tree.bitextract[0] = 0b1000000000100000000;

    cout << "SDFSDF" << endl;
    cout << extract_bits(&test_node.tree, A) << endl;*/

    //let's test inserting bit in mask and extraction together
    /*add_position_to_extraction_mask(&test_node.tree, 64);
    print_binary_uint64(test_node.tree.byte_extract, true);
    print_binary_uint64(test_node.tree.bitextract[0], true);
    cout << extract_bits(&test_node.tree, A) << endl;
    add_position_to_extraction_mask(&test_node.tree, 35);
    print_binary_uint64(test_node.tree.byte_extract, true);
    print_binary_uint64(test_node.tree.bitextract[0], true);
    cout << extract_bits(&test_node.tree, A) << endl;
    add_position_to_extraction_mask(&test_node.tree, 448);
    print_binary_uint64(test_node.tree.byte_extract, true);
    print_binary_uint64(test_node.tree.bitextract[0], true);
    print_binary_uint64(test_node.tree.bitextract[1], true);
    cout << extract_bits(&test_node.tree, A) << endl;
    add_position_to_extraction_mask(&test_node.tree, 1);
    print_binary_uint64(test_node.tree.byte_extract, true);
    print_binary_uint64(test_node.tree.bitextract[0], true);
    print_binary_uint64(test_node.tree.bitextract[1], true);
    cout << extract_bits(&test_node.tree, A) << endl;
    add_position_to_extraction_mask(&test_node.tree, 76);
    print_binary_uint64(test_node.tree.byte_extract, true);
    print_binary_uint64(test_node.tree.bitextract[0], true);
    print_binary_uint64(test_node.tree.bitextract[1], true);
    cout << extract_bits(&test_node.tree, A) << endl;

    add_position_to_extraction_mask(&test_node.tree, 9);
    print_binary_uint64(test_node.tree.byte_extract, true);
    print_binary_uint64(test_node.tree.bitextract[0], true);
    print_binary_uint64(test_node.tree.bitextract[1], true);
    cout << extract_bits(&test_node.tree, A) << endl;
    add_position_to_extraction_mask(&test_node.tree, 17);
    print_binary_uint64(test_node.tree.byte_extract, true);
    print_binary_uint64(test_node.tree.bitextract[0], true);
    print_binary_uint64(test_node.tree.bitextract[1], true);
    cout << extract_bits(&test_node.tree, A) << endl;
    add_position_to_extraction_mask(&test_node.tree, 27);
    print_binary_uint64(test_node.tree.byte_extract, true);
    print_binary_uint64(test_node.tree.bitextract[0], true);
    print_binary_uint64(test_node.tree.bitextract[1], true);
    cout << extract_bits(&test_node.tree, A) << endl;
    add_position_to_extraction_mask(&test_node.tree, 95);

    print_binary_uint64(test_node.tree.byte_extract, true);
    print_binary_uint64(test_node.tree.bitextract[0], true);
    print_binary_uint64(test_node.tree.bitextract[1], true);
    cout << extract_bits(&test_node.tree, A) << endl;*/

    /*insert(&test_node, A);
    print_binary_uint64(test_node.tree.byte_extract, true);
    print_binary_uint64(test_node.tree.bitextract[0], true);
    print_binary_uint64(test_node.tree.bitextract[1], true);
    print_vec(test_node.tree.treebits, true, 16);
    print_vec(test_node.ignore_mask, true, 16);
    insert(&test_node, B);
    print_binary_uint64(test_node.tree.byte_extract, true);
    print_binary_uint64(test_node.tree.bitextract[0], true);
    print_binary_uint64(test_node.tree.bitextract[1], true);
    print_vec(test_node.tree.treebits, true, 16);
    print_vec(test_node.ignore_mask, true, 16);

    cout << _tzcnt_u32(1) << endl;*/

    //The Real Test
    int numtests=10;
    constexpr int sizetests=1;
    int numfailed=0;
    int failedindex=-1;
    int testindex=-1;
    for(int i=0; i<numtests; i++) {
        memcpy(&test_node, &Empty_Fusion_Node, sizeof(fusion_node));
        __m512i randomlist[sizetests];
        //vector<vector<uint64_t>> randomlistvec(16);
        vector<int> positions = generate_random_positions(generator, sizetests);
        for(int j=0; j<sizetests; j++) {
            uniform_int_distribution<uint64_t> temporary_distribution(0, 1);
            if(temporary_distribution(generator))
                randomlist[j] = gen_vec_one_bit(positions[j]);
            else 
                randomlist[j] = gen_random_vec(generator);
        }
        if(testindex!=-1 && testindex!=i) continue;

        for(int j=0; j<sizetests; j++) {
            /*cout << "Inserting: ";
            print_vec(randomlist[j], true);*/
            insert(&test_node, randomlist[j]);
            /*print_node_info(test_node);
            cout << "Inserted the " << j << "th thing" << endl;*/
        }
        /*for(int j=0; j<sizetests; j++) {
            print_binary_uint64_big_endian(randomlist[j][7], true, 64, 8);
        }*/
        /*for(int j=0; j<sizetests; j++) {
            cout << first_diff_bit_pos(B, randomlist[j]) << endl;
        }*/
        sort(randomlist, randomlist+sizetests, compare__m512i);
        /*for(int j=0; j<sizetests; j++) {
            print_vec(randomlist[j], true);
        }*/
        for(int j=0; j<sizetests; j++){
            int branch = query_branch(&test_node, randomlist[j]);
            if(branch >= 0) {
                //cout << "Failed test " << i << ", and didn't even think the key was there" << endl;
                numfailed++;
                failedindex=i;
                break;
            }
            branch = ~branch;
            if(branch != j) {
                //cout << "Failed test " << i << " at node " << j << endl;
                numfailed++;
                failedindex=i;
                break;
            }
        }
    }
    cout << "Num failed: " << numfailed << ", and one failed index (if any) is " << failedindex << endl;
    
    constexpr long long bigtestsize = 10000000;
    __m512i* big_randomlist = static_cast<__m512i*>(std::aligned_alloc(64, bigtestsize*64));
    uint64_t* small_randomlist = (uint64_t*)malloc(bigtestsize*sizeof(uint64_t));
    set<uint64_t> list_set;
    boost::container::set<uint64_t> boost_set;
    //cout << "DFSDFDS" << endl;
    fusion_b_node* root = NULL;
    //set<__m512i, decltype(compare__m512i)*, allocator<__m512i>> randomlist_set;
    for(int i=0; i < bigtestsize; i++) {
    	//cout << "inserting " << i << "th vector" << endl;
    	big_randomlist[i] = gen_random_vec(generator);
        small_randomlist[i] = gen_random_uint64(generator);
    	//randomlist_set.insert(big_randomlist[i]);
    	//root = insert_full_tree(root, big_randomlist[i]);
    	//print_binary_uint64_big_endian(big_randomlist[i][7], true, 64, 8);
    	//print_binary_uint64_big_endian(big_randomlist[i][7], true);
    	//print_vec(big_randomlist[i], true);
    	//cout << ((int)root->fusion_internal_tree.tree.meta.size) << endl;
    	//printTree(root);
    }
    auto start = chrono::high_resolution_clock::now();
    for(int i=0; i < bigtestsize; i++) {
    	root = insert_full_tree(root, big_randomlist[i]);
    }
    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::microseconds>(end-start);
    cout << "Time to insert: " << duration.count() << endl;
    
    start = chrono::high_resolution_clock::now();
    for(int i=0; i < bigtestsize; i++) {
    	list_set.insert(small_randomlist[i]);
    }
    end = chrono::high_resolution_clock::now();
    duration = chrono::duration_cast<chrono::microseconds>(end-start);
    cout << "Time to insert 64 bit ints to std::set: " << duration.count() << endl;

    start = chrono::high_resolution_clock::now();
    for(int i=0; i < bigtestsize; i++) {
    	boost_set.insert(small_randomlist[i]);
    }
    end = chrono::high_resolution_clock::now();
    duration = chrono::duration_cast<chrono::microseconds>(end-start);
    cout << "Time to insert 64 bit ints to boost::container::set: " << duration.count() << endl;

    start = chrono::high_resolution_clock::now();
    sort(big_randomlist, big_randomlist+bigtestsize, compare__m512i);
    end = chrono::high_resolution_clock::now();
    duration = chrono::duration_cast<chrono::microseconds>(end-start);
    cout << "Time to sort with std::sort : " << duration.count() << endl;
    
    /*start = chrono::high_resolution_clock::now();
    for(int i=0; i < bigtestsize; i++) {
    	root = insert_full_tree(root, big_randomlist[i]);
    }
    end = chrono::high_resolution_clock::now();
    duration = chrono::duration_cast<chrono::microseconds>(end-start);
    cout << "Time to insert: " << duration.count() << endl;*/
    
    __m512i prev = {0};
    start = chrono::high_resolution_clock::now();
    for(int i=0; i < bigtestsize; i++) {
    	prev = *successor(root, prev);
    	//assert(first_diff_bit_pos(prev, big_randomlist[i]) == -1);
    }
    end = chrono::high_resolution_clock::now();
    duration = chrono::duration_cast<chrono::microseconds>(end-start);
    cout << "Time to retrieve sorted list with fusion tree: " << duration.count() << endl;
    //sort(big_randomlist, big_randomlist+bigtestsize, compare__m512i);
    /*for(int i=0; i < bigtestsize; i++) {
    	root = insert_full_tree(root, big_randomlist[i]);
    }*/
    for(int i=0; i < bigtestsize-1; i++) {
    	//cout << "Searching ";
    	//print_binary_uint64_big_endian(big_randomlist[i][7], true, 64, 8);
    	__m512i* treesucc = successor(root, big_randomlist[i]);
    	if(treesucc == NULL){
    		cout << "treesuc is NULL" << endl;
            break;
    	}
    	else {
    		if(first_diff_bit_pos(*treesucc, big_randomlist[i+1]) != -1) {
    			cout << "Wrong at " << i << endl;
    			print_binary_uint64_big_endian((*treesucc)[7], true, 64, 8);
    			break;
		}
    	}
    }

    for(int i=bigtestsize-1; i > 0 ; i--) {
    	//cout << "Searching ";
    	//print_binary_uint64_big_endian(big_randomlist[i][7], true, 64, 8);
    	__m512i* treepred = predecessor(root, big_randomlist[i]);
    	if(treepred == NULL){
    		cout << "treepred is NULL" << endl;
            break;
    	}
    	else {
    		if(first_diff_bit_pos(*treepred, big_randomlist[i-1]) != -1) {
    			cout << "Wrong pred at " << i << endl;
                print_binary_uint64_big_endian(big_randomlist[i-1][7], true, 64, 8);
    			print_binary_uint64_big_endian((*treepred)[7], true, 64, 8);
    			break;
		}
    	}
    }
    //printTree(root);
    cout << "max depth: " << maxDepth(root) << endl;
    cout << "random seed is " << seed << endl;
}

