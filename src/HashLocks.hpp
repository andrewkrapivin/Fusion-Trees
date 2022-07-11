#ifndef HASHLOCKS_INCLUDED
#define HASHLOCKS_INCLUDED

#include "Locks.hpp"
#include <vector>
#include <array>

//Really simple fix to all these problems:
//Just since doing HOH locking, have like say four hash tables where the lcoks are for depth mod 4, so then you don't have any deadlocking problems.
//And here you don't care about running out of space in an entry at all.


//Biggest potential problem: deadlocking & adjacent nodes (parent/child) colliding?
//For deadlock: like say one thread does P-C, and trying to get CC 
//Another thread has CC-CCC and trying to get CCCC
//Then what if the hash of CCCC is the same as, say, the hash of P?
//Then thread 1 waits for thread 2 to give up CCC, and thread 2 waits for thread 1 to give up P---deadlock
//One solution is have a separate hash table for each level, but idk about that. Seems dubious, but might be reasonable
//This solution uses a lot of space that seems unnecessary.
//Second solution: have metadata in each lock storing say something like level or something, and have a few buckets in each hash table
//So that if a lock is being held with a lower number than the one you have, you request another lock.
//If no locks in the bucket available with a lower number, then well you're screwed? Or maybe smth along the lines of linear probing?
//Solution: really just lock not with a binary 0 or 1 but with an id of the item that you want. Then basically if both cuckoo buckets are full, you just fail.
//Or can do a smarter thing where part of the id is the level of the node, so then if trying to lock something and your node level is lower (or higher, if consistent to do only higher) you can wait for the lock to free, and otherwise you fail.
//So that you only wait for locks "down." but this behavior might be implemented in the client code, certainly for the comparison of levels, but maybe with some helper funcs here.

//Scheme is designed for the hash locks, obviously
//So readlocks and write locks can be in one of two locations---
//Thus whenever you try to lock a read lock, it is given two possible write lock locations to check
//Whenever you try to lock a write lock, it is given a vector of possible read lock locations
//Honestly is this a good idea? Idk maybe its better to just have combined locks, which could be less memory efficient but
//Less mem efficient cause total locks needs to be proportional to threads, so then uses O(t^2) space where t is threadcount, since size of each lock proportional to O(t) (cachelines)
//In this scheme each thread gets constant number of readlocks per thread, and only the writelocks are proportional to threads
//So you use O(t)+O(t) = O(t) space.

// constexpr size_t NumThreads = 4;
// constexpr size_t LocksPerThread = 10;
// constexpr size_t LocksPerLayer = numThreads * locksPerThread;
//Since size of lock is proportional to threads, this has a scaling problem of using O(t^2) space where t is threadcount.
//Should numThreads be constant time or runtime? Idk. Feels like constant should work but runtime should be better so that say can query cpucount @ runtime

enum class TryLockPossibilities {
    Success,
    WriteLocked, //Returns if the actual write lock you want (that is, the one with the same id) is held
    LocksBusy, //Returns if both slots in the cuckoo table are filled with locks that are not the one you want
    Error //if lock already held or required lock not held (by the thread, so this would be like double locking or trying to upgrade without having readlock). This should probably be an exception actually since it should never happen, but for now just let it be an error code
};

enum class LockType {
    Unlocked,
    WriteLocked,
    ReadLocked,
    PartiallyWriteLocked
};

class BasicHashFunction {
    private:
        //Idk what hash function to use. Let's do this super space inneficient one, which seems pretty good though even on theoretical basis? Idk about that but well it certainly has a lot of random bits
        //Lol but 2KB for just 8 bits of randomness at the end is absurd. Whatever
        //Def improve this if plan to use it more than like once
        static constexpr size_t keySize = 8; //Idk why I have this. if care about key size prob better to template or make it in constructor
        size_t numBits;
        size_t numBytes;
        std::vector<std::array<std::array<unsigned char, 256>, keySize>> shuffleBytes; //actually ridiculous
    
    public:
        BasicHashFunction(size_t numBits);
        uint64_t getBits(uint64_t id);
        size_t operator() (uint64_t id);
        
};

class SimpleHashFunction {
    private:
        //Idk what hash function to use. Let's do this super space inneficient one, which seems pretty good though even on theoretical basis? Idk about that but well it certainly has a lot of random bits
        //Lol but 2KB for just 8 bits of randomness at the end is absurd. Whatever
        //Def improve this if plan to use it more than like once
        static constexpr size_t maxbits = 30;
        size_t numBits;
        size_t a, b;
    
    public:
        SimpleHashFunction(size_t numBits);
        uint64_t getBits(uint64_t id);
        size_t operator() (uint64_t id);
        
};

//This lock hash table is designed for head over head locking or any similar scheme (each "rung" of the HOH scheme gets its own hash table, so three total):
//where basically each lock hash table will be used for just ONE lock per thread, and deadlocks arising from hashing two locks to the same location in a hash table cannot arise
//(in HOH, we would have three hash tables, and even if two different nodes hash to the same entry we do not create a lock loop since we do not effectively lock deep enough to cause a problem--better explained in a diagram)
//One thing maybe still todo here is doing as the LockHashTable having separate read and write locks just to reduce memusage.
class SimpleLockHashTable {
    private:
        static constexpr size_t sizeOverhead = 10;
        size_t numThreads;
        size_t numBits;
        std::vector<LockUnit> writeLocks;
        std::vector<LockUnit> readLocks; //lol don't just have one readlock per thread cause then writelocking becomes super expensive!!!
        SimpleHashFunction hashFunc;
        void getWriteLock(size_t id);
        TryLockPossibilities tryGetWriteLock(size_t id);
        void waitForReadLocks(size_t id);
        

    public:
        SimpleLockHashTable() = delete;
        SimpleLockHashTable(size_t numThreads);
        void writeLock(size_t id);
        TryLockPossibilities tryWriteLock(size_t id);
        void partialUpgrade(size_t id, size_t threadId);
        TryLockPossibilities tryPartialUpgrade(size_t id, size_t threadId, bool unlockOnFail = true);
        void finishPartialUpgrade(size_t id);
        void readLock(size_t id, size_t threadId);
        TryLockPossibilities tryReadLock(size_t id, size_t threadId);
        void writeUnlock(size_t id);
        void partialUpgradeUnlock(size_t id);
        void readUnlock(size_t id, size_t threadId);
};

//Build in scheme to rebuild the hash table? Cause honestly probably most of the time you have the LocksBusy thing happen the locks you use later change and so its all good, but sometimes its gonna be two popular locks colliding, and then I'd imagine that's a problem.
//Or some deadlock situtation could also happen, which is the real problem.
//The scheme would just be to have a global lock for the hash table, and make every function get read access for it
class LockHashTable {
    private:
        static constexpr float sizeOverhead = 2.0; //inverse of load factor. Keeping it const for now but probably make it configurable 
        // size_t numWriteLocksHeld{0};
        size_t numWriteBits, numReadBits;
        // vector<LockUnit> writeLockModifyLocks1; //Terrible name lol. These are used to do the Cuckoo insertions, since here we need to move around. Not for deletions. Design requires this, which is why cuckoo was chosen
        // vector<LockUnit> writeLockModifyLocks2;
        // vector<LockUnit> writeLocks1; //two vectors for the two tables with cuckoo
        // vector<LockUnit> writeLocks2;
        std::vector<WriteMutex> lockWriteLocks;
        // size_t associativity;
        size_t associativityCacheLines;
        std::vector<PackedLockUnit> writeLocks; //Actually this can be achieved without PackedLockUnit & it would be at least somewhat faster? Not sure about this cause gotta stream more memory then but less contention
        std::vector<std::vector<PackedLockUnit>> readLocks; //Here its probably ideal to do PackedLockUnit but not sure cause it still could slow down the writers
        //readLocks have the first  dimension is set by the threadId, then next is the actual read lock we want
        // BasicHashFunction h1, h2;
        SimpleHashFunction hashFunc;
        struct HashIds {
            // uint64_t writeEntry1, writeEntry2;
            // uint64_t readEntry1, readEntry2;
            // uint64_t writeEntry, readEntry;
            // LockUnit& wlockLock1, wlockLock2;
            // LockUnit& wlock1, wlock2;
            uint64_t writeLockEntry, lockWriteLockEntry, readLockEntry;
            HashIds(LockHashTable* h, size_t id);
        };

        void getWriteLock(size_t id);
        TryLockPossibilities tryGetWriteLock(size_t id);
        void waitForReadLocks(size_t id);
        std::atomic<uint64_t>* getReadLock(size_t id, size_t threadId); //so awful
        std::atomic<uint64_t>* tryGetReadLock(size_t id, size_t threadId, TryLockPossibilities& status);

    public:
        LockHashTable(size_t numThreads, size_t locksPerThread, size_t associativity = 3); //default associativity should be 8
        void writeLock(size_t id);
        TryLockPossibilities tryWriteLock(size_t id); //Change the size_t here to uint64_t since I use that to index everything. Idk why I chose size_t in the first place, or choose size_t for the atomic units.
        void partialUpgrade(size_t id, size_t threadId);
        //If unlockOnFail is set to true, this will unlock the lock no matter the type of error, even if it is some error of the hash table entry being full or whatever
        TryLockPossibilities tryPartialUpgrade(size_t id, size_t threadId, bool unlockOnFail = true);
        void finishPartialUpgrade(size_t id);
        void readLock(size_t id, size_t threadId);
        TryLockPossibilities tryReadLock(size_t id, size_t threadId);
        void writeUnlock(size_t id);
        void partialUpgradeUnlock(size_t id);
        void readUnlock(size_t id, size_t threadId);
};

//Template this? Since have several different hash table designs.
class HashMutex {
    private:
        SimpleLockHashTable* table;
        size_t id;

    public:
        HashMutex() {} //Delete later when done transitioning
        HashMutex(SimpleLockHashTable* table, size_t id);
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
        size_t getId();

};

//Change this to add a constructor from HashMutex? Not really a big change but really shouldn't matter much anyways
class HashLock {
    private:
        LockHashTable* table; //Seems I need to make this a pointer to allow for the move assignment operator? Cause it seems to try to move the references or something? And move constructions can and should not be defined for this.
        size_t id;
        size_t threadId;
        LockType curLockType;
        // HashReadLock(LockHashTable& table, size_t id, size_t threadId, TryLockPossibilities status);
    
    public:
        HashLock(LockHashTable* table, size_t id, size_t threadId);
        ~HashLock();
    
        HashLock(const HashLock&) = delete;
        HashLock& operator=(const HashLock&) = delete;
        
        HashLock(HashLock&&);
        HashLock& operator=(HashLock&&);

        void writeLock();
        TryLockPossibilities tryWriteLock();
        void partialUpgrade();
        TryLockPossibilities tryPartialUpgrade(bool unlockOnFail = true);
        void finishPartialUpgrade();
        void readLock();
        TryLockPossibilities tryReadLock();
        void writeUnlock();
        void partialUpgradeUnlock();
        void readUnlock();
        void unlock(); //Automatically just sees what it needs to unlock.

};

//Encapsulating stuff like HOH locking, which would work as so in the B-trees:
//Every node needs to know its depth d
//Then we have three hash tables, and when locking a node we lock lockTables[d%3]
//This ensures that the up to three locks we hold at a time in a thread do not cause deadlock
//Do not know if there are any other applications of this, but just making the constant 3 configurable. And anyways made it power of two to make the mod operation more efficient, since 3 is really a minimum
class StripedLockTable {
    private:
        std::vector<SimpleLockHashTable> lockTables;
    
    public:
        StripedLockTable(size_t numThreads, size_t numLocksPerThread = 3);
        HashMutex getMutex(size_t id, size_t stripeId);

};

#endif