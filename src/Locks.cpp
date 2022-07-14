#include "Locks.hpp"
#include <iostream>
#include <cassert>

PackedLockUnit::PackedLockUnit() {
    for(std::atomic<uint64_t>& lockId: lockIds) {
        lockId.store(0, std::memory_order_release);
    }
}


void WriteMutex::lock() {
    // Should I use compare_exchange or exchange? Cause here can have weaker memory order on fail, so could be faster?
    // while(wlUnit.lockId.exchange(1, std::memory_order_acquire) == 0) {
    //     while(wlUnit.lockId.load(std::memory_order_relaxed) == 0);
    // }
    uint64_t expected = 0;
    while(!wlUnit.lockId.compare_exchange_weak(expected, 1, std::memory_order_acquire, std::memory_order_relaxed)) {
        expected = 0;
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
    for(auto& rlUnit: rlUnits) {
        while(rlUnit.lockId.load(std::memory_order_relaxed) != 0);
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
    rlUnits[threadId].lockId.load(std::memory_order_seq_cst);

    while(wlUnit.lockId.load(std::memory_order_relaxed) != 0) {
        rlUnits[threadId].lockId.store(0, std::memory_order_relaxed);
        while(wlUnit.lockId.load(std::memory_order_relaxed) != 0);
        rlUnits[threadId].lockId.load(std::memory_order_seq_cst);
    }
    // std::cout << "BYE2" << std::endl;
}

bool ReadWriteMutex::tryReadLock(size_t threadId) {
    rlUnits[threadId].lockId.load(std::memory_order_seq_cst);

    // std::cout << "HELLO5 " << threadId << std::endl;

    if(wlUnit.lockId.load(std::memory_order_relaxed) != 0) {
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
    // rlUnits[threadId].lockId.store(0, std::memory_order_relaxed); //I think relaxed is fine here? Should test this on a system without strong memory ordering so not x86
    // Might actually not be ok since do we want to allow reordering of loads after this store!! Maybe we need acquire barrier here. Although that applies to all the locks in general?? Or rather we actually need a release barrier here.
    // Possible problem:
    // Imagine that we want to use a data structure protected by a lock. We then just want to save some info from the data structure.
    // Then the loads for some of the info are reordered after and some before, so some info is old and some is new, which might just completely break teh data structure.
    // But also this then actually should be a release barrier since this "saving" of data is a store to new mem location, and that is the only problem we are concerned with. So no other locks not broken, just this one technically was (although I do not think it would be broken for fusion trees since I do not do this saving of data that I do not then have some other lock solve the problem for me with?)
    rlUnits[threadId].lockId.store(0, std::memory_order_release);
    //This feels unnecessary since there is a (currently? implicit) contract that if you have a readlock you cannot store to the protected data, so you don't need to worry about ordering stores before, but by the argument above we need it.
}

// void HashReadLock::readlock(int thread, uint64_t id, std::array<HashWriteLock&,2> possibleWriteLocks) {
    
// }

// void HashReadLock::tryReadlock(int thread, uint64_t id, std::array<HashWriteLock&,2> possibleWriteLocks) {
    
// }