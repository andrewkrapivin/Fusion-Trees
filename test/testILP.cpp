#include <random>
#include <chrono>
#include <iostream>

#include "../src/fusion_tree.h"
#include "../src/BenchHelper.h"
#include "../src/HelperFuncs.h"

using namespace std;

constexpr int buffsize = 1280;
__m512i buff1[buffsize];
int ibuff[buffsize];
__mmask16 maskbuff[buffsize];
void first_diff_bit_pos_buff(std::array<__m512i, buffsize>& x, std::array<__m512i, buffsize>& y, std::array<int, buffsize>& retvals) {
    __m512i zero = {0, 0, 0, 0, 0, 0, 0, 0};
    for(int i =0; i < buffsize; i++) {
        buff1[i] = _mm512_xor_si512(x[i], y[i]);
    }
    for(int i=0; i < buffsize; i++) {
        maskbuff[i] = _mm512_cmp_epi32_mask(zero, buff1[i], _MM_CMPINT_NE);
    }
    for(int i=0; i < buffsize; i++) {
        retvals[i] = _cvtmask16_u32(maskbuff[i]);
        retvals[i] = retvals[i] == 31 - _lzcnt_u32(retvals[i]); //not sure what to do with this, just remember that 31 is undesired
        //should work cause 1 << 31 in 16 bit mask should be 0 and then the next process works but is a waste but whatever
    }
    for(int i=0; i < buffsize; i++) {
        maskbuff[i] = _cvtu32_mask16(1 << retvals[i]);
    }
    for(int i=0; i < buffsize; i++) {
        buff1[i] = _mm512_maskz_compress_epi32(maskbuff[i], buff1[i]);
    }
    for(int i=0; i < buffsize; i++) {
        __m128i lowerbytes = _mm512_extracti64x2_epi64(buff1[i], 0);
        ibuff[i] = _mm_extract_epi32(lowerbytes, 0);
    }
    for(int i=0; i < buffsize; i++) {
        if(retvals[i] == 31) {
            retvals[i] = -1;
        }
        else {
            retvals[i] = 32*retvals[i] + (31 - _lzcnt_u32(ibuff[i]));
        }
    }
}

int main() {
    constexpr size_t bigtestsize = 20000;
    unsigned seed = chrono::steady_clock::now().time_since_epoch().count();
    mt19937 generator (seed);

    size_t* rperm = new size_t[bigtestsize];
    for(int i=0; i < bigtestsize; i++) {
        rperm[i] = i;
    }
    shuffle(rperm, rperm+bigtestsize, generator);

    size_t* rperm2 = new size_t[bigtestsize*buffsize];
    for(int i=0; i < bigtestsize*buffsize; i++) {
        rperm2[i] = i;
    }
    shuffle(rperm2, rperm2+bigtestsize*buffsize, generator);
    
    array<__m512i, buffsize>* x = new array<__m512i, buffsize>[bigtestsize];
    array<__m512i, buffsize>* y = new array<__m512i, buffsize>[bigtestsize];
    array<int, buffsize>* retvals = new array<int, buffsize>[bigtestsize];
    __m512i* xdf = (__m512i*)(x);
    __m512i* ydf = (__m512i*)(x);

    for(int i=0; i < bigtestsize; i++) {
        for(int j=0; j < buffsize; j++) {
    	    x[i][j] = gen_random_vec(generator);
            y[i][j] = gen_random_vec(generator);
        }
    }

    BenchHelper bench;

    bench.timeFunction([&] () -> void
    {for(int i=0; i < bigtestsize; i++) {
        first_diff_bit_pos_buff(x[rperm[i]], y[rperm[i]], retvals[rperm[i]]);
    }}, "ILPbuffered first diff bit pos");

    bench.timeFunction([&] () -> void
    {for(int i=0; i < bigtestsize; i++) {
        for(int j=0; j < buffsize; j++) {
            first_diff_bit_pos(x[rperm[i]][j], y[rperm[i]][j]);
        }
    }}, "unILPbuffered first diff bit pos");

    bench.timeFunction([&] () -> void
    {for(size_t i=0; i < bigtestsize*buffsize; i++) {
        first_diff_bit_pos(xdf[rperm2[i]], ydf[rperm2[i]]);
    }}, "unILPbuffered first diff bit pos");

    //unbuffered is 2x faster! LOOLO>OLOLOLOLOLOLODFKDSJFOSDIJFO. Can be 3x faster actually lololoolllololololol if not using array (using xdf sequentially) lol.

    delete x;
    delete y;
    delete retvals;
    delete rperm;
}