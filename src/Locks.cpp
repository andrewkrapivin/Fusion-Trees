#include "Locks.hpp"
#include <iostream>
#include <cassert>

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
    assert(wlUnit.lockId.load() == 0);
    for(auto& x: rlUnits) {
        assert(x.lockId.load() == 0);
    }
}

void ReadWriteMutex::getWriteLock() {
    // std::cout << "HELLO" << std::endl;
    uint64_t expected = 0;
    while(!wlUnit.lockId.compare_exchange_weak(expected, 1, std::memory_order_acquire, std::memory_order_relaxed)) {
        expected = 0;
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
    // std::cout << "HELLO6" << std::endl;
    getWriteLock();
    readUnlock(threadId);
    // std::cout << "BYE6" << std::endl;
}

bool ReadWriteMutex::tryPartialUpgrade(size_t threadId, bool unlockOnFail /* = true */) {
    // std::cout << "HELLO7" << std::endl;
    uint64_t expected = 0;
    if(!wlUnit.lockId.compare_exchange_strong(expected, 1, std::memory_order_acquire, std::memory_order_relaxed)) {
        if(unlockOnFail) {
            readUnlock(threadId);
        }
        // std::cout << "BADBYE7" << std::endl;
        return false;
    }
    readUnlock(threadId);
    // std::cout << "BYE7" << std::endl;
    return true;
}

void ReadWriteMutex::finishPartialUpgrade() {
    waitForReadLocks();
}

void ReadWriteMutex::readLock(size_t threadId) {
    // std::cout << "HELLO2 " << threadId << std::endl;
    rlUnits[threadId].lockId.store(1, std::memory_order_seq_cst);

    while(wlUnit.lockId.load(std::memory_order_acquire) != 0) {
        rlUnits[threadId].lockId.store(0, std::memory_order_relaxed);
        while(wlUnit.lockId.load(std::memory_order_relaxed) != 0);
        rlUnits[threadId].lockId.store(1, std::memory_order_seq_cst);
    }
    // std::cout << "BYE2" << std::endl;
}

bool ReadWriteMutex::tryReadLock(size_t threadId) {
    rlUnits[threadId].lockId.store(1, std::memory_order_seq_cst);

    // std::cout << "HELLO5 " << threadId << std::endl;

    if(wlUnit.lockId.load(std::memory_order_acquire) != 0) {
        rlUnits[threadId].lockId.store(0, std::memory_order_relaxed);
        return false;
    }

    // std::cout << "BYE5" << std::endl;

    return true;
}

void ReadWriteMutex::writeUnlock() {
    wlUnit.lockId.store(0, std::memory_order_release);
}

void ReadWriteMutex::partialUpgradeUnlock() {
    wlUnit.lockId.store(0, std::memory_order_release);
}

void ReadWriteMutex::readUnlock(size_t threadId) {
    // std::cout << "HELLO3 " << threadId << std::endl;
    rlUnits[threadId].lockId.store(0, std::memory_order_release); //I think relaxed is fine here? Should test this on a system without strong memory ordering so not x86
}

// void HashReadLock::readlock(int thread, uint64_t id, std::array<HashWriteLock&,2> possibleWriteLocks) {
    
// }

// void HashReadLock::tryReadlock(int thread, uint64_t id, std::array<HashWriteLock&,2> possibleWriteLocks) {
    
// }