#ifndef PARALLEL_B_TREE_H_INCLUDED
#define PARALLEL_B_TREE_H_INCLUDED

#include "Locks.hpp"
#include <thread>
#include <fstream>
#include <ostream>
#include <immintrin.h>
#include "HashLocks.hpp"

struct ParallelBNode {
    static constexpr size_t MaxSize = 16;
    __m512i keys[MaxSize];
    size_t curSize = 0;
    ParallelBNode();
    void insert(__m512i key);
    void insert(__m512i key, size_t pos);
    bool full();
    size_t query(__m512i key);
    static bool split(ParallelBNode* cur, ParallelBNode* par);
    ParallelBNode* children[MaxSize+1];
    void deleteSubtrees();
};

class ParallelBTree {
    private:
        size_t numThreads;
        StripedLockTable lockTable;
        ParallelBNode root;
        std::vector<std::ofstream> debugFiles;

    public:
        ParallelBTree(size_t numThreads);
        void insert(__m512i key, size_t threadId);
        __m512i* successor(__m512i key, size_t threadId);
        __m512i* predecessor(__m512i key, size_t threadId);
        ~ParallelBTree();
        ParallelBTree(const ParallelBTree&) = delete;
        ParallelBTree& operator=(const ParallelBTree&) = delete;
};

class ParallelBTreeThread {
    private:
        ParallelBTree& tree;
        size_t threadId;
    
    public:
        ParallelBTreeThread(ParallelBTree& tree, size_t threadId);
        void insert(__m512i key);
        __m512i* successor(__m512i key);
        __m512i* predecessor(__m512i key);
};

#endif
