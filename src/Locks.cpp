#include "Locks.hpp"

void WriteMutex::lock() {
    // Should I use compare_exchange or exchange? Cause here can have weaker memory order on fail, so could be faster?
    while(wlUnit.lockId.exchange(1, std::memory_order_acquire) == 0) {
        while(wlUnit.lockId.load(std::memory_order_relaxed) == 0);
    }
}

bool WriteMutex::tryLock() {
    uint64_t expected = 0;
    return wlUnit.lockId.compare_exchange_strong(expected, 1, std::memory_order_acquire, std::memory_order_relaxed);
}

void WriteMutex::unlock() {
    wlUnit.lockId.store(0, std::memory_order_release);
}



ReadWriteMutex::ReadWriteMutex(size_t numThreads): rlUnits(numThreads) {
}

void ReadWriteMutex::getWriteLock() {
    while(wlUnit.lockId.exchange(1, std::memory_order_acquire) == 0) {
        while(wlUnit.lockId.load(std::memory_order_relaxed) == 0);
    }
}

void ReadWriteMutex::waitForReadLocks() {
    for(auto& rlunit: rlUnits) {
        while(rlunit.lockId.load(std::memory_order_relaxed) != 0);
    }
}

void ReadWriteMutex::writeLock() {
    getWriteLock();
    waitForReadLocks();
}

bool ReadWriteMutex::tryWriteLock() {
    // if(wlUnit.lockId.exchange(1, std::memory_order_acquire) != 0) return false;
    uint64_t expected = 0;
    //Should I use compare_exchange or exchange? Cause here can have weaker memory order on fail, so could be faster?
    if(wlUnit.lockId.compare_exchange_strong(expected, 1, std::memory_order_acquire, std::memory_order_relaxed) != 0) return false;

    waitForReadLocks();
    return true;
}

void ReadWriteMutex::partialUpgrade(size_t threadId) {
    getWriteLock();
    readUnlock(threadId);
}

bool ReadWriteMutex::tryPartialUpgrade(size_t threadId, bool unlockOnFail /* = true */) {
    uint64_t expected = 0;
    if(!wlUnit.lockId.compare_exchange_strong(expected, 1, std::memory_order_acquire, std::memory_order_relaxed)) {
        if(unlockOnFail) {
            readUnlock(threadId);
        }
        return false;
    }
    readUnlock(threadId);
    return true;
}

void ReadWriteMutex::finishPartialUpgrade() {
    waitForReadLocks();
}

void ReadWriteMutex::readLock(size_t threadId) {
    rlUnits[threadId].lockId.store(1, std::memory_order_release);

    while(wlUnit.lockId.load(std::memory_order_acquire) != 0) {
        rlUnits[threadId].lockId.store(0, std::memory_order_relaxed);
        while(wlUnit.lockId.load(std::memory_order_relaxed) != 0);
        rlUnits[threadId].lockId.store(1, std::memory_order_release);
    }
}

bool ReadWriteMutex::tryReadLock(size_t threadId) {
    rlUnits[threadId].lockId.store(1, std::memory_order_release);

    if(wlUnit.lockId.load(std::memory_order_acquire) != 0) {
        rlUnits[threadId].lockId.store(0, std::memory_order_relaxed);
        return false;
    }

    return true;
}

void ReadWriteMutex::writeUnlock() {
    wlUnit.lockId.store(0, std::memory_order_release);
}

void ReadWriteMutex::partialUpgradeUnlock() {
    wlUnit.lockId.store(0, std::memory_order_release);
}

void ReadWriteMutex::readUnlock(size_t threadId) {
    rlUnits[threadId].lockId.store(0, std::memory_order_relaxed); //I think relaxed is fine here? Should test this on a system without strong memory ordering so not x86
}

// void HashReadLock::readlock(int thread, uint64_t id, std::array<HashWriteLock&,2> possibleWriteLocks) {
    
// }

// void HashReadLock::tryReadlock(int thread, uint64_t id, std::array<HashWriteLock&,2> possibleWriteLocks) {
    
// }