#include "HelperFuncs.h"
#include <iostream>

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