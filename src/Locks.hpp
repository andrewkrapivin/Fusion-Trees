#ifndef LOCKS_INCLUDE
#define LOCKS_INCLUDE

#include <atomic>
#include <cstdint>
#include <array>
#include <vector>

struct alignas(64) LockUnit {
    std::atomic<uint64_t> lockId{0};
    uint64_t padding[7];

    // void lock();
    // void unlock();
};

struct alignas(64) PackedLockUnit { //This struct should be a typedef I'm being very hacky here. There is no reason to not make it typedef
    static constexpr size_t numLocks = 8;
    using container = std::array<std::atomic<uint64_t>, numLocks>;
    using iterator=typename container::iterator;
    using const_iterator=typename container::const_iterator;
    // std::atomic<uint64_t> lockId[numLocks];
    container lockIds = {};
    iterator begin() {
        return lockIds.begin();
    }
    iterator end() {
        return lockIds.end();
    }
    const_iterator cbegin() const { 
        return lockIds.cbegin();
    }
    const_iterator cend() const {
        return lockIds.cend();
    }
};

class alignas(64) WriteMutex {
    private:
        LockUnit wlUnit;

    public:
        void lock();
        bool tryLock();
        void unlock();
};

class alignas(64) ReadWriteMutex {
    private:
        LockUnit wlUnit; //writelock
        std::vector<LockUnit> rlUnits; //readlocks
        void getWriteLock();
        void waitForReadLocks();

    public:
        ReadWriteMutex(size_t numThreads);
        void writeLock();
        bool tryWriteLock();
        void partialUpgrade(size_t threadId);
        bool tryPartialUpgrade(size_t threadId, bool unlockOnFail = true);
        void finishPartialUpgrade();
        void readLock(size_t threadId);
        bool tryReadLock(size_t threadId);
        void writeUnlock();
        void partialUpgradeUnlock();
        void readUnlock(size_t threadId);

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