#include "HashLocks.hpp"

BasicHashFunction::BasicHashFunction(size_t numBits): numBits(numBits), numBytes((numBits+7)/8), seedBytes(numBytes) {
    //Since this hash function is crazy let's also go nuts with randomness. Have no idea whether rd actually has enough entropy for this to give any theoretical benefit, so whatever. Let's also add the clock to it then.
    std::random_device rd;
    std::seed_seq seed{rd(), rd(), rd(), rd(), rd(), std::chrono::steady_clock::now().time_since_epoch().count()};
    std::mt19937 generator{seed};
    // std::uniform_int_distribution dist{0, 255};
    for(auto a: shuffleBytes) {
        for(auto b: a) {
            //Should this be a permutation of 0-255 or just random bytes? Probably shuffle is a little better but a little less entropy but then might not even use all the bits so not a real diff? Idk
            std::array<unsigned char, 256> permutation;
            for(size_t i{0}; i < 256; i++) permutation[i] = i;
            std::shuffle(permutation.begin(), permutation.end(), generator);
            // for(size-t i{0}; i < 256; i++) {
            //     // a[i] = dist(generator);
            // }
        }
    }
}

//seriously this needs to be made at least more efficient
uint64_t BasicHashFunction::getBits(uint64_t id) {
    uint64_t res = 0;

    unsigned char entriesToShuffle[keySize] = std::bit_cast<unsigned char[keySize]>(id); //Wack this bit_cast thing is.

    uint64_t shift = 0;
    for(auto a: shuffleBytes) {
        for(size_t i{0}; i < keySize; i++) {
            res ^= a[i][entriesToShuffle[i]] << shift;
        }
        shift+=8;
    }
    res &= (1ull << numBits) - 1;
    return res;
}

size_t BasicHashFunction::operator() (uint64_t id) {
    return getBits(id);
}

LockHashTable::HashIds(LockHashTable* h, size_t id) {
    // writeEntry = h->hashFunc(id);
    // readEntry = writeEntry & ((1ull << h->numReadBits) - 1);

    // writeEntry1 = h->h1(id);
    // writeEntry2 = h->h2(id);
    
    // readEntry1 = writeEntry1 & ((1ull << h->numReadBits) - 1);
    // readEntry2 = writeEntry2 & ((1ull << h->numReadBits) - 1);

    lockWriteLockEntry = h->hashFunc(id);
    readLockEntry = (lockWriteLockEntry & ((1ull << h->numReadBits) - 1)) * associativityCacheLines;
    writeLockEntry = lockWriteLockEntry * associativityCacheLines;
}

LockHashTable::LockHashTable(size_t numThreads, size_t locksPerThread, size_t associativity /*= 8*/): associativityCacheLines{(associativity+PackedLockUnit::numLocks-1)/PackedLockUnit::numLocks} {
    // associativity = associativityCacheLines * PackedLockUnit::numLocks;
    size_t numReadLocksPerThread = lockPerThread * sizeOverhead;
    size_t numWriteLocks = numThreads * numReadLocksPerThread;
    numWriteBits = 64 - _lzcnt_u64(numReadLocksPerThread-1);
    numReadBits = 64 - _lzcnt_u64(numWriteLocks-1);
    hashFunc{numWriteBits}; //Just gonna do linear probing. Nice & easy.
    // h1{numWriteBits};
    // h2{numWriteBits};
    numReadLocksPerThread = 1ull << numWriteBits; //taking the smallest power of 2 at least equal to the desired amount
    numWriteLocks = 1ull << numReadBits;
    // writeLocks1 = vector<LockUnit>(numWriteLocks);
    // writeLocks2 = vector<LockUnit>(numWriteLocks);
    // writeLockModifyLocks1 = vector<LockUnit>(numWriteLocks);
    // writeLockModifyLocks2 = vector<LockUnit>(numWriteLocks);
    lockWriteLocks = vector<LockUnit>(numWriteLocks);
    writeLocks = vector<PackedLockUnit>(numWriteLocks*associativityCacheLines);
    readLocks  = vector<vector<PackedLockUnit>>(numThreads);
    for(size_t i{0}; i < numThreads; i++) {
        readLocks[i] = vector<PackedLockUnit>(numReadLocksPerThread*associativityCacheLines); //These obviously don't need to be as big since as opposed to having up to threadCount*locksPerThread items in the hash table at a time, it just has locksPerThread, but for now this is as it is
    }
}

void LockHashTable::getWriteLock(size_t id) {
    HashIds hIds(this, id);

    LockUnit& wlockLock = writeLockModifyLocks[hIds.lockWriteLockEntry];
    wlockLock.lock(); //lock lock lock

    bool acquiredLock = false;
    // size_t newVal = id;
    // size_t oldVal = 0;
    for(size_t cl{0}; cl < associativityCacheLines; cl++) {
        PackedLockUnit& writeLock = writeLocks[hIds.writeLockEntry+cl];
        for(auto& x: writeLock.lockIds) {
            // oldVal = x.exchange(newVal, std::memory_order_relaxed);
            uint64_t val = x.load(std::memory_order_relaxed);
            if(val == id) {
                wlockLock.unlock();
                while(x.load(std::memory_order_relaxed) == id);
                return getWriteLock(id);
            }
            if(val == 0) {
                x.store(id, std::memory_order_relaxed);
                acquiredLock = true;
                break;
            }
            // newVal = oldVal;
        }
        if(acquiredLock) {
            break;
        }
    }

    if(!acquiredLock) {
        exit(EXIT_FAILURE);  //Don't do this. Rebuild the hash table lol or something in this case. Or just do something to wait for locks to be released.
    }

    wlockLock.unlock(); //This releases so all the relaxed commits should be visible to others
}

TryLockPossibilities LockHashTable::tryGetWriteLock(size_t id) {
    HashIds hIds(this, id);

    LockUnit& wlockLock = writeLockModifyLocks[hIds.lockWriteLockEntry];
    wlockLock.lock();

    bool acquiredLock = false;
    for(size_t cl{0}; cl < associativityCacheLines; cl++) {
        PackedLockUnit& writeLock = writeLocks[hIds.writeLockEntry+cl];
        for(auto& x: writeLock.lockIds) {
            uint64_t val = x.load(std::memory_order_relaxed);
            if(val == id) {
                wlockLock.unlock();
                return TryLockPossibilities::WriteLocked;
            }
            if(val == 0) {
                x.store(id, std::memory_order_relaxed);
                acquiredLock = true;
                break;
            }
        }
        if(acquiredLock) {
            break;
        }
    }

    if(!acquiredLock) {
        return TryLockPossibilities::LocksBusy;
    }

    wlockLock.unlock();

    return TryLockPossibilities::Success;
}

void LockHashTable::waitForReadLocks(size_t id) {
    HashIds hIds(this, id);

    for(auto& readLockThread: readLocks) {
        for(size_t cl{0}; cl < associativityCacheLines; cl++) {
            PackedLockUnit& readLock = readLockThread[hIds.readLockEntry+cl];
            for(auto& rl: readLock) {
                uint64_t val;
                do {
                    val = rl.load(std::memory_order_relaxed);
                    if(val == 0) goto NEXTLOCK; //Policy for readers is to ALWAYS have the first zero be done so as for this part to be a bit more efficient. Basically this means that new read lock must be added at the end always, which is obv less efficient but the hope is that load here will be pretty small.
                } while(val == id);
            }
        }
        NEXTLOCK: ;
    }
}

std::atomic<uint64_t>& LockHashTable::getReadLock(size_t id, size_t threadId) {
    HashIds hIds(this, id);
    bool gotLock = false;
    for(size_t cl{0}; cl < associativityCacheLines; cl++) {
        PackedLockUnit& readLock = readLockThread[hIds.readLockEntry+cl];
        for(auto& rl: readLock) {
            uint64_t expected = 0;
            if(rl.compare_exchange_strong(expected, id, std::memory_order_release, std::memory_order_relaxed)) { //Not sure about the memory order here!! But the logic is that we want this to have a happens-before relationship with the acquire that the write lock does, so that the write lock is guaranteed to see this & does not go along changing stuff incorrectly
                return rl;
            }
        }
    }

    exit(EXIT_FAILURE); //again lol
    return nullptr; //Is this line necessary?
}

std::atomic<uint64_t>& LockHashTable::tryGetReadLock(size_t id, size_t threadId, TryLockPossibilities& status) {
    HashIds hIds(this, id);
    bool gotLock = false;
    for(size_t cl{0}; cl < associativityCacheLines; cl++) {
        PackedLockUnit& readLock = readLockThread[hIds.readLockEntry+cl];
        for(auto& rl: readLock) {
            uint64_t expected = 0;
            if(rl.compare_exchange_strong(expected, id, std::memory_order_release, std::memory_order_relaxed)) {
                status = TryLockPossibilities::Success;
                return rl;
            }
        }
    }

    status = TryLockPossibilities::LocksBusy;
    return nullptr;
}

//Read locks a thread can move around at will with no problems, as long as it makes sure that other threads see the changes for write.
//For write locks moving them around make it a "lock for locks--" have a second write lock hash table where you lock the location(s) you are changing while operating on the hash table--prevents deadlocks

//This function can introduce deadlocks even where there would be none without the hash table, but should be rare.
void LockHashTable::writeLock(size_t id) {

    // LockUnit& wlock1 = writeLocks[hIds.writeEntry1];
    // LockUnit& wlock2 = writeLocks[hIds.writeEntry2];
    // //Assuming I did this correctly, what it should do at the end is modify exactly one of the entries in the hash table with the id, and then on success make sure release is followed. Not entirely sure if being relaxed on failure is ok?
    // while(!wlock1.compare_exchange_weak(0, id, std::memory_order_acquire, std::memory_order_relaxed) && !wlock2.compare_exchange_weak(0, id, std::memory_order_acquire, std::memory_order_relaxed));

    // for(auto& readLockThread : readLocks) {
    //     LockUnit& rlock1 = readLockThread[hIds.readEntry1];
    //     LockUnit& rlock2 = readLockThread[hIds.readEntry2];

    //     while(rlock1.load(std::memory_order_relaxed) == id);
    //     while(rlock2.load(std::memory_order_relaxed) == id);
    // }

    // LockUnit& wlockLock = writeLockModifyLocks[hIds.writeEntry2];
    // // wlockLock1.lock(); //Lock lock lock
    // wlockLock.lock(); //Lock lock lock
    // LockUnit& wlock = writeLocks2[hIds.writeEntry2];
    // if(wlock.lockId.load(std::memory_order_relaxed) != id) {
    //     LockUnit& wlockLock = writeLockModifyLocks[hIds.writeEntry1];
    //     wlockLock1.lock();
    // }
    // if(wlock1.lockId.load(std::memory_order_relaxed) == id || wlock2.lockId.load(std::memory_order_relaxed) == id);
    // uint64_t oldId;
    // while((oldId = wlock1.lockId.exchange(id, std::memory_order_relaxed)) == id) {
    //     while(wlock1.lockId.load(std::memory_order_relaxed) == id);//"atomic exchange operation requires write access to the cache line where the lock is stored," so we just want to read while waiting for it to be freed
    // }

    getWriteLock(id);
    waitForReadLocks(id);
    
}

TryLockPossibilities LockHashTable::tryWriteLock(size_t id) {
    TryLockPossibilities retval = tryGetWriteLock(id);
    if(retval != TryLockPossibilities::Success) {
        return retval;
    }

    waitForReadLocks(id);

    return TryLockPossibilities::Success;
}

void LockHashTable::partialUpgrade(size_t id, size_t threadId) {
    getWriteLock(id);
    readUnlock(id, threadId);
}

TryLockPossibilities LockHashTable::tryPartialUpgrade(size_t id, size_t threadId, bool unlockOnFail = true) {
    TryLockPossibilities retval = tryGetWriteLock(id);
    if(retval != TryLockPossibilities::Success) {
        if(unlockOnFail) {
            readUnlock(id, threadId);
        }
        return retval;
    }

    readUnlock(id, threadId);
    return TryLockPossibilities::Success;
}

void LockHashTable::finishPartialUpgrade(size_t id) {
    waitForReadLocks(id);
}

void LockHashTable::readLock(size_t id, size_t threadId) {
    std::atomic<uint64_t>& readLock = getReadLock(id, threadId);

    HashIds hIds(this, id); //should probably make this class like an actual entry or something to not recalculate this twice, unless compiler optimizes it away

    RESTART_WRITE_LOCK_WAIT: for(size_t cl{0}; cl < associativityCacheLines; cl++) {
        PackedLockUnit& writeLock = writeLocks[hIds.writeLockEntry+cl];
        for(auto& x: writeLock.lockIds) {
            uint64_t val = x.load(std::memory_order_acquire); //I think this needs to be acquire since a deleting thread may copy the id lock to the previous lock, and then this does not see that change but then sees the next change here which copies the next lock to this lock
            if(val == id) {
                // readLock.store(0, std::memory_order_release);
                readLock.store(0, std::memory_order_relaxed); //Not 1000% sure what memory orders to do here. But I think this works.
                while(x.load(std::memory_order_relaxed) == id);
                readLock.store(1, std::memory_order_release);
                goto RESTART_WRITE_LOCK_WAIT;
            }
            if(val == 0) {
                return;
            }
        }
    }
}

TryLockPossibilities LockHashTable::tryReadLock(size_t id, size_t threadId) {
    TryLockPossibilities status = TryLockPossibilities::Success;
    std::atomic<uint64_t>& readLock = tryGetReadLock(id, threadId, status);
    if(status != TryLockPossibilities::Success) {
        return status;
    }

    HashIds hIds(this, id);

    for(size_t cl{0}; cl < associativityCacheLines; cl++) {
        PackedLockUnit& writeLock = writeLocks[hIds.writeLockEntry+cl];
        for(auto& x: writeLock.lockIds) {
            uint64_t val = x.load(std::memory_order_acquire);
            if(val == id) {
                readLock.store(0, std::memory_order_release);
                return TryLockPossibilities::WriteLocked;
            }
            if(val == 0) {
                return TryLockPossibilities::Success;
            }
        }
    }

    return TryLockPossibilities::Success;
}

void LockHashTable::writeUnlock(size_t id) {
    HashIds hIds(this, id);

    LockUnit& wlockLock = writeLockModifyLocks[hIds.lockWriteLockEntry];
    wlockLock.lock();

    std::atomic<uint64_t>& prevVal = nullptr;
    for(size_t cl{0}; cl < associativityCacheLines; cl++) {
        PackedLockUnit& writeLock = writeLocks[hIds.writeLockEntry+cl];
        for(auto& x: writeLock.lockIds) {
            if(prevVal != nullptr) {
                prevVal.store(x.load(std::memory_order_relaxed), std::memory_order_release); //Don't this that it can be relaxed, cause might mess up read threads.
            }
            else if(x.load(std::memory_order_relaxed) == id) {
                prevVal = x;
            }
        }
    }

    wlockLock.unlock();
}

void LockHashTable::partialUpgradeUnlock(size_t id) { //actually the same (for now) as write unlock
    writeUnlock(id);
}

void LockHashTable::readUnlock(size_t id, size_t threadId) {
    HashIds hIds(this, id);
    bool gotLock = false;
    for(size_t cl{0}; cl < associativityCacheLines; cl++) {
        PackedLockUnit& readLock = readLockThread[hIds.readLockEntry+cl];
        for(auto& rl: readLock) {
            uint64_t expected = id;
            if(rl.compare_exchange_strong(expected, 0, std::memory_order_release, std::memory_order_relaxed)) {
                return;
            }
        }
    }
}

// //Probably combine this with writeLock and a simple if. Only minor problem is the return statements right now
// TryLockPossibilities LockHashTable::tryWriteLock(size_t id) {
//     HashIds hIds(this, id);

//     LockUnit& wlock1 = writeLocks[hIds.writeEntry1];
//     LockUnit& wlock2 = writeLocks[hIds.writeEntry2];

//     uint64_t val1 = 0;
//     uint64_t val2 = 0;
//     if(!wlock1.compare_exchange_strong(val1, id, std::memory_order_acquire, std::memory_order_relaxed) && !wlock2.compare_exchange_strong(val2, id, std::memory_order_acquire, std::memory_order_relaxed)) {
//         if(val1 == id || val2 == id) {
//             return TryLockPossibilities::WriteLocked;
//         }
//         return TryLockPossibilities::LocksBusy;
//     }

//     for(auto& readLockThread : readLocks) {
//         LockUnit& rlock1 = readLockThread[hIds.readEntry1];
//         LockUnit& rlock2 = readLockThread[hIds.readEntry2];

//         while(rlock1.lockId.load(std::memory_order_relaxed) == id);
//         while(rlock2.lockId.load(std::memory_order_relaxed) == id);
//     }

//     return TryLockPossibilities::Success;
// }

// void LockHashTable::partialUpgrade(size_t id, size_t threadId) {
//     HashIds hIds(this, id);

//     LockUnit& wlock1 = writeLocks[hIds.writeEntry1];
//     LockUnit& wlock2 = writeLocks[hIds.writeEntry2];

//     while(!wlock1.compare_exchange_weak(0, id, std::memory_order_acquire, std::memory_order_relaxed) && !wlock2.compare_exchange_weak(0, id, std::memory_order_acquire, std::memory_order_relaxed));
// }

// TryLockPossibilities LockHashTable::tryPartialUpgrade(size_t id, size_t threadId, bool unlockOnFail = true)  {

// }
//     HashIds hIds(this, id);

//     LockUnit& wlock1 = writeLocks[hIds.writeEntry1];
//     LockUnit& wlock2 = writeLocks[hIds.writeEntry2];

//     uint64_t val1 = 0;
//     uint64_t val2 = 0;
//     if(!wlock1.compare_exchange_strong(val1, id, std::memory_order_acquire, std::memory_order_relaxed) && !wlock2.compare_exchange_strong(val2, id, std::memory_order_acquire, std::memory_order_relaxed)) {
//         if(unlockOnFail) {
//             readUnlock(id, threadId);
//         }
//         if(val1 == id || val2 == id) {
//             return TryLockPossibilities::WriteLocked;
//         }
//         return TryLockPossibilities::LocksBusy;
//     }

//     readUnlock(id, threadId);

//     return TryLockPossibilities::Success;
// }

// void finishPartialUpgrade(size_t id) {
//     HashIds hIds(this, id);

//     for(auto& readLockThread : readLocks) {
//         LockUnit& rlock1 = readLockThread[hIds.readEntry1];
//         LockUnit& rlock2 = readLockThread[hIds.readEntry2];

//         while(rlock1.lockId.load(std::memory_order_relaxed) == id);
//         while(rlock2.lockId.load(std::memory_order_relaxed) == id);
//     }
// }

// void LockHashTable::readLock(size_t id, size_t threadId) {
//     HashIds hIds(this, id);

//     LockUnit& rlock1 = readLocks[threadId][hIds.readEntry1];
//     LockUnit& rlock2 = readLocks[threadId][hIds.readEntry2];

//     //(remember that read lock is unique to thread so it is still exclusive effectively, unless there is some programmer mistake, but that applies to writelocks as well)
//     while(!rlock1.lockId.compare_exchange_weak(0, id, std::memory_order_acquire, std::memory_order_relaxed) && !rlock1.lockId.compare_exchange_weak(0, id, std::memory_order_acquire, std::memory_order_relaxed));

//     LockUnit& wlock1 = writeLocks[hIds.writeEntry1];
//     LockUnit& wlock2 = writeLocks[hIds.writeEntry2];
//     while(wlock1.lockId.load(std::memory_order_relaxed) == id);
//     while(wlock2.lockId.load(std::memory_order_relaxed) == id);
// }

// TryLockPossibilities LockHashTable::tryReadLock(size_t id, size_t threadId) {
//     HashIds hIds(this, id);

//     LockUnit& rlock1 = readLocks[threadId][hIds.readEntry1];
//     LockUnit& rlock2 = readLocks[threadId][hIds.readEntry2];

//     if(!rlock1.lockId.compare_exchange_weak(0, id, std::memory_order_acquire, std::memory_order_relaxed) && !rlock1.lockId.compare_exchange_weak(0, id, std::memory_order_acquire, std::memory_order_relaxed)) {
//         return TryLockPossibilities::LocksBusy;
//     }

//     LockUnit& wlock1 = writeLocks[hIds.writeEntry1];
//     LockUnit& wlock2 = writeLocks[hIds.writeEntry2];
//     while(wlock1.lockId.load(std::memory_order_relaxed) == id);
//     while(wlock2.lockId.load(std::memory_order_relaxed) == id);
// }


// HashReadLock::HashReadLock(LockHashTable& table, size_t id, size_t threadId, TryLockPossibilities status) : table(table), id(id), threadId(threadId), status(status) {
//     holdingLock = status == TryLockPossibilities::Success;
// }

HashLock::HashLock(LockHashTable& table, size_t id, size_t threadId): table(table), id(id), threadId(threadId), curLockType(LockType::Unlocked) {
}

HashLock::HashLock(HashLock&& a): table(a.table), id(a.id), threadId(a.threadId), curLockType(a.curLockType) {
    a.table = nullptr;
    a.id = 0;
    a.threadId = -1;
    a.curLockType = LockType::Unlocked;
}

HashLock::HashLock& operator=(HashLock&& a) {
    switch(curLockType) {
        default: break;
        case LockType::WriteLocked: 
            writeUnlock();
            break;
        case LockType::ReadLocked:
            readUnlock();
            break;
        case LockType::PartiallyWriteLocked:
            partialUpgradeUnlock();
            break;
    }

    //Seems this code is a bit repetitive with the move one? Combine somehow?
    table = a.table;
    id = a.id;
    threadId = a.threadId;
    curLockType = a.curLockType;

    a.table = nullptr;
    a.id = 0;
    a.threadId = -1;
    a.curLockType = LockType::Unlocked;
}

HashLock::~HashLock() {
    unlock();
}

//Here I guess switch based on type. Get a write lock no matter what.
void HashLock::writeLock() {
    switch(curLockType) {
        case LockType::Unlocked:
            table.writeLock(id);
            break;
        case LockType::ReadLocked:
            partialUgrade();
            finishPartialUpgrade();
            break;
        case LockType::PartiallyWriteLocked:
            finishPartialUpgrade();
            break;
        case LockType::WriteLocked:
            break;
    }
    curLockType = LockType::WriteLocked;
}

//Here I'm gonna say its an error to get a write lock unless you are unlocked
TryLockPossibilities HashLock::tryWriteLock() {
    if(curLockType != LockType::Unlocked) return TryLockPossibilities::Error;

    TryLockPossibilities t =  table.tryWriteLock(id);
    if(t == TryLockPossibilities::Success) {
        curLockType = LockType::WriteLocked;
    }
    return t;
}

//Again saying you have to get it. Idk about this function imo.
TryLockPossibilities HashLock::partialUpgrade() {
    switch(curLockType) {
        case LockType::Unlocked: //esp this is dumb
            readLock();
            partialUgrade();
            curLockType = LockType::PartiallyWriteLocked;
            break;
        case LockType::ReadLocked:
            partialUgrade();
            curLockType = LockType::PartiallyWriteLocked;
            break;
        case LockType::PartiallyWriteLocked:
            break;
        case LockType::WriteLocked:
            break;
    }
}

TryLockPossibilities HashLock::LockHashTable::tryPartialUpgrade(bool unlockOnFail)  {
    if(curLockType != LockType::ReadLocked) return TryLockPossibilities::Error; //maybe put this behind a debug constexpr or something

    TryLockPossibilities t = table.LockHashTable::tryPartialUpgrade(id, threadId, unlockOnFail) {

    }
    if(t == TryLockPossibilities::Success) {
        curLockType = LockType::PartiallyWriteLocked;
    }
    else if (unlockOnFail) {
        curLockType = LockType::Unlocked;
    }
    return t;
}

void HashLock::finishPartialUpgrade() {
    if(curLockType == LockType::PartiallyWriteLocked) return; //idk maybe actually start throwing errors around and stuff or something

    table.finishPartialUpgrade(id);
    curLockType = LockType::WriteLocked;
}

void HashLock::readLock() {
    if(curLockType != LockType::Unlocked) return;

    readLock();
    curLockType = LockType::ReadLocked;
}

TryLockPossibilities HashLock::tryReadLock() {
    if(curLockType != LockType::Unlocked) return TryLockPossibilities::Error;

    TryLockPossibilities t = table.tryReadLock(id, threadId);
    if(t == TryLockPossibilities::Success) {
        curLockType = LockType::ReadLocked;
    }
    return t;
}

void HashLock::unlock() {
    switch(curLockType) {
        default: break;
        case LockType::WriteLocked: 
            table.writeUnlock(id);
            break;
        case LockType::ReadLocked:
            table.readUnlock(id, threadId);
            break;
        case LockType::PartiallyWriteLocked:
            table.partialUpgradeUnlock(id);
            break;
    }
}
// void writeUnlock();
// void partialUpgradeUnlock();
// void readUnlock();