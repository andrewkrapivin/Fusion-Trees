#ifndef HELPER_FUNCS_INCLUDED
#define HELPER_FUNCS_INCLUDED

#include <immintrin.h>
#include <cstdint>
#include <random>
#include "fusion_tree.h"

using namespace std; //seriously don't do this lol

void print_binary_uint64(uint64_t x, bool newline=false, int divider=64);
void print_binary_uint64_big_endian(uint64_t x, bool newline=false, int divider=64, int numbits=64);
void print_hex_uint64(uint64_t x, bool newline=false, int divider=64);
void print_vec(__m512i X, bool binary, int divider=64);
void print_vec(__m256i X, bool binary, int divider=64);
void print_vec(__m128i X, bool binary, int divider=64);
uint64_t gen_random_uint64(mt19937& generator);
__m512i gen_random_vec(mt19937& generator);
__m512i gen_random_vec_one_bit(mt19937& generator);
vector<int> generate_random_positions(mt19937& generator, int count, int maxpos=511);
__m512i gen_vec_one_bit(int bit_pos);
__m512i gen_random_vec_all_but_one_bit(mt19937& generator);
void print_node_info(fusion_node& test_node);

#endif 