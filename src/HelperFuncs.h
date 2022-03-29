#ifndef HELPER_FUNCS_INCLUDED
#define HELPER_FUNCS_INCLUDED

#include <immintrin.h>
#include <cstdint>

using namespace std; //seriously don't do this lol

void print_binary_uint64(uint64_t x, bool newline=false, int divider=64);
void print_binary_uint64_big_endian(uint64_t x, bool newline=false, int divider=64, int numbits=64);
void print_hex_uint64(uint64_t x, bool newline=false, int divider=64);
void print_vec(__m512i X, bool binary, int divider=64);
void print_vec(__m256i X, bool binary, int divider=64);
void print_vec(__m128i X, bool binary, int divider=64);

#endif 