#include "fusion_tree.h"

#include <random>
#include <chrono>
#include <iostream>
#include <climits>
#include <immintrin.h>
#include <cstdint>
#include <algorithm>

__m512i gen_random_vec(mt19937 generator) {
    __m512i A;
    std::uniform_int_distribution<uint64_t> temporary_distribution(0, ULLONG_MAX);
    for(int i=0; i < 8; i++){
        A[i] = temporary_distribution(generator);
    }
    return A;
}

void print_binary_uint64(uint64_t x, bool newline=false) {
    for(int i=0; i < 64; i++) {
        cout << (x%2);
        x/=2;
    }
    if(newline) cout << endl;
}

void print_hex_uint64(uint64_t x, bool newline=false) {
    char digit_to_char[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    for(int i=0; i < 16; i++) {
        cout << digit_to_char[x%16];
        x/=16;
    }
    if(newline) cout << endl;
}

void print_vec(__m512i X, bool binary) { //either prints binary or hex in little endian fashion because of weirdness
    if(binary) cout << "0b ";
    else cout << "0x ";
    for(int i=0; i < 8; i++) {
        if(binary)
            print_binary_uint64(X[i]);
        else 
            cout << X[i];
        cout << ' ';
    }
    cout << endl;
}

void print_vec(__m256i X, bool binary) { //either prints binary or hex in little endian fashion because of weirdness
    if(binary) cout << "0b ";
    else cout << "0x ";
    for(int i=0; i < 4; i++) {
        if(binary)
            print_binary_uint64(X[i]);
        else 
            cout << X[i];
        cout << ' ';
    }
    cout << endl;
}

int main(){
    unsigned seed = chrono::steady_clock::now().time_since_epoch().count();
    mt19937 generator (seed);

	__m512i A = {0, 1, 0, 0, 0, 0, 0, 1}; //we treat the number as little endian, cause otherwise it is inconsistent.
    //Cause the CPU stores each number as little endian, and for let's say the 65th digit to be one, we initialize it as in A, which is really weird to think about, because the earlier numbers are more significant as normal, but then within the numbers its the opposite, but, well, whatever
    //Little/big endian is pretty annoying
	__m512i B = {0, 2, 0, 0, 0, 0, 0, 0};
	
	__m512i X = gen_random_vec(generator);
    print_vec(A, true);

    print_binary_uint64(65, true);
    print_hex_uint64(65, true);
    uint64_t x = 65;
    
	cout << _lzcnt_u32(1) << endl;
	
	cout << first_diff_bit_pos(A, B) <<" afdaff" <<  endl;

    __m256i test = {0};

    print_vec(setbit_each_epi16_in_range(test, 2, 0, 7, 1), true);

    //testing stuff
    fusion_node test_node = {0};
    cout << insert(&test_node, X) << endl;

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

    insert(&test_node, A);
    insert(&test_node, B);
    print_binary_uint64(test_node.tree.byte_extract, true);
    print_binary_uint64(test_node.tree.bitextract[0], true);
    print_binary_uint64(test_node.tree.bitextract[1], true);
    print_vec(test_node.tree.treebits, true);
    print_vec(test_node.ignore_mask, true);

}

