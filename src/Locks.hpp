#ifndef LOCKS_INCLUDE
#define LOCKS_INCLUDE

#include <atomic>
#include <cstdint>
#include <array>

struct alignas(64) LockUnit {
    std::atomic<uint64_t> lockId;
    uint64_t padding[7];

    void lock();
    void unlock();
};

struct alignas(64) PackedLockUnit { //This struct should be a typedef I'm being very hacky here. There is no reason to not make it typedef
    static constexpr size_t numLocks = 8;
    using container = std::array<std::atomic<uint64_t>, numLocks>;
    using iterator=typename container::iterator;
    using const_iterator=typename container::const_iterator;
    // std::atomic<uint64_t> lockId[numLocks];
    container lockIds = {};
};


//Idk the interfaces here are a little meh, maybe improve this idk or somehow join write& read/just have operations directly on lockunit that can either be for reading or writing
// class HashWriteLock {
//     private:
//         LockUnit lock;
    
//     public:
//         bool locked(uint64_t id);

// };

// class HashReadLock {
//     private:
//         LockUnit lock;
    
//     public:
//         void readlock(int thread, uint64_t id, std::array<HashWriteLock&,2> possibleWriteLocks);

//         tryLockPossibilities tryReadlock(int thread, uint64_t id, std::array<HashWriteLock&,2> possibleWriteLocks);
//         void waitForUnlock(uint64_t id);

// };

#endif