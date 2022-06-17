#include "Locks.hpp"

void LockUnit::lock() {
    while(lockId.exchange(1, std::memory_order_acquire)) {
        while(lockId.load(std::memory_order_relaxed));
    }
}

void LockUnit::unlock() {
    lockId.store(0, std::memory_order_release);
}

// void HashReadLock::readlock(int thread, uint64_t id, std::array<HashWriteLock&,2> possibleWriteLocks) {
    
// }

// void HashReadLock::tryReadlock(int thread, uint64_t id, std::array<HashWriteLock&,2> possibleWriteLocks) {
    
// }