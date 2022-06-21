#ifndef THREADED_ID_GENERATOR_INCLUDED
#define THREADED_ID_GENERATOR_INCLUDED

#include <cstdint>
#include <vector>

//Doing this just in case. Cause multiple writers have to go fetch the cache line to then be able to read it. I think this is better but might use more bandwidth?
//Can be used to give multiple different counters cayse well each thread doesn't care, so consider adding this functionality.
struct alignas(64) CounterUnit {
    uint64_t counter;
    uint64_t padding[7];
};

//Generates unique nonzero ids in threaded manner
//Really simple. Essentially just splits up uint64_t space into numThreads, assuming that since 2^64 is so big splitting that up among threads doesn't matter
class ThreadedIdGenerator {
    private:
        std::vector<CounterUnit> counters;
    
    public:
        ThreadedIdGenerator(size_t numThreads);
        uint64_t operator() (size_t threadId);
};

#endif