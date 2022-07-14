#include "ThreadedIdGenerator.hpp"
#include <iostream>

ThreadedIdGenerator::ThreadedIdGenerator(size_t numThreads): counters(numThreads) {
    uint64_t spaceEachCounter = (-1ull) / numThreads;
    uint64_t val = 1;
    for(auto&x : counters) {
        x.counter = val;
        val += spaceEachCounter;
    }
}

uint64_t ThreadedIdGenerator::operator() (size_t threadId) {
    // std::cout << "new node: " <<counters[threadId].counter << std::endl;
    return counters[threadId].counter++;
}