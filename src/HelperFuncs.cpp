#include "HelperFuncs.h"
#include <iostream>
#include <climits>
#include <set>

void print_binary_uint64(uint64_t x, bool newline /*=false*/, int divider /*=64*/) {
    for(int i=0; i < 64; i++) {
        if(i>0 && i%divider == 0) cout << ' ';
        cout << (x%2);
        x/=2;
    }
    if(newline) cout << endl;
}

void print_binary_uint64_big_endian(uint64_t x, bool newline /*=false*/, int divider /*=64*/, int numbits /*=64*/) {
    for(int i=63, j=0; i >= 0 && j < numbits; i--, j++) {
        if(i>0 && i%divider == 0) cout << ' ';
        cout << (((1ull << i) & x) != 0);
    }
    if(newline) cout << endl;
}

void print_hex_uint64(uint64_t x, bool newline /*=false*/, int divider /*=64*/) {
    char digit_to_char[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    for(int i=0; i < 16; i++) {
        if(i>0 && i%divider == 0) cout << ' ';
        cout << digit_to_char[x%16];
        x/=16;
    }
    if(newline) cout << endl;
}

void print_vec(__m512i X, bool binary, int divider /*=64*/) { //either prints binary or hex in little endian fashion because of weirdness
    if(binary) cout << "0b ";
    else cout << "0x ";
    for(int i=0; i < 8; i++) {
        if(binary)
            print_binary_uint64(X[i], false, divider);
        else 
            cout << X[i];
        cout << ' ';
    }
    cout << endl;
}

void print_vec(__m256i X, bool binary, int divider /*=64*/) { //either prints binary or hex in little endian fashion because of weirdness
    if(binary) cout << "0b ";
    else cout << "0x ";
    for(int i=0; i < 4; i++) {
        if(binary)
            print_binary_uint64(X[i], false, divider);
        else 
            cout << X[i];
        cout << ' ';
    }
    cout << endl;
}

void print_vec(__m128i X, bool binary, int divider /*=64*/) { //either prints binary or hex in little endian fashion because of weirdness
    if(binary) cout << "0b ";
    else cout << "0x ";
    for(int i=0; i < 2; i++) {
        if(binary)
            print_binary_uint64(X[i], false, divider);
        else 
            cout << X[i];
        cout << ' ';
    }
    cout << endl;
}

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
    return A;
}

vector<int> generate_random_positions(mt19937& generator, size_t count, int maxpos /*=511*/){
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