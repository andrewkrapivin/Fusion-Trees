#ifndef SIMPLE_ALLOC_H_INCLUDED
#define SIMPLE_ALLOC_H_INCLUDED

#include <cstdlib>
#include <iostream>
#include "fusion_tree.h"

// typedef struct fusion_bf_node{
// 	fusion_node fusion_internal_tree;
// 	fusion_bf_node* children[MAX_FUSION_SIZE+1];
//     fusion_bf_node* parent;
//     //temporary debug vals
//     bool visited;
//     uint64_t id;
// } fusion_bf_node;

//Simply allocates a whole array of same sized things with an allignment;

template <class T, size_t alignment>
class SimpleAlloc {
    private:
        size_t capacity;
        size_t numFreeElements;
        T* array_pointer;
        T** free_elements;
    public:
        SimpleAlloc(size_t capacity);
        ~SimpleAlloc();
        T* alloc();
        void free(T* ptr);
};

    template <class T, size_t alignment> 
    SimpleAlloc<T, alignment>::SimpleAlloc(size_t capacity) {
        this->capacity = capacity;
        this->numFreeElements = capacity;
        array_pointer = (T*)std::aligned_alloc(alignment, capacity*sizeof(T));
        free_elements = (T**)malloc(capacity*sizeof(T*));
        for(size_t i=0; i < capacity; i++) {
            free_elements[i] = (array_pointer+capacity-i-1);
        }
    }

    template <class T, size_t alignment> 
    SimpleAlloc<T, alignment>::~SimpleAlloc() {
        std::free(array_pointer);
        std::free(free_elements);
    }

    template <class T, size_t alignment> 
    T* SimpleAlloc<T, alignment>::alloc() {
        if(numFreeElements == 0) return NULL;
        return free_elements[--numFreeElements];
    }

    template <class T, size_t alignment> 
    void SimpleAlloc<T, alignment>::free(T* ptr) {
        free_elements[numFreeElements++] = ptr;
        // cout << "Freeing " << ((fusion_bf_node*)ptr)->id << endl;
    }

#endif