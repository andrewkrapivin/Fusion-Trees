#ifndef HASHTABLE_INCLUDED
#define HASHTABLE_INCLUDED

#include <cstdint>
#include <cstddef>
#include <vector>
#include <atomic>
#include <array>
#include "Locks.hpp"
#include "HashLocks.hpp"
#include <immintrin.h>

// constexpr size_t MLPSize = 8; //number cachelines prefetchable at once. Not sure what this should actually be
// constexpr size_t EntriesPerBucket = 4;

// typedef uint64_t HashType;
// typedef uint32_t SHashType;

// template<typename NT>
// struct alignas(64) HashBucket {
//     // std::shared_mutex mtx;
//     uint8_t offsets[EntriesPerBucket]; //4 bytes
//     // uint32_t shashSeed; //4 bytes
//     SHashType shashes[EntriesPerBucket]; //16 bytes
//     NT* nodeptrs[EntriesPerBucket]; //32 bytes
//     //Total: 56 bytes-- (almost) one cache line for four entries
// };

// constexpr size_t numEachSide = (MLPSize/2 - 1) * 64 / (EntriesPerBucket * (sizeof(HashType) + sizeof(SHashType)));

// template<typename NT>
// struct alignas(64) MLPHashBucket {
//     HashBucket<NT> b;
//     HashType successorHashes[EntriesPerBucket][numEachSide];
//     SHashType successorSHashes[EntriesPerBucket][numEachSide];
//     HashType predecessorHashes[EntriesPerBucket][numEachSide];
//     SHashType predecessorSHashes[EntriesPerBucket][numEachSide];
// };

//Gonna assume that the size of VT is small enough for everything other than the key to fit in one cache line, which is probably not a good assumption to make. But we can make it bigger to a certain extent.
template<typename NT, typename VT>
struct alignas(64) HashBucket {
    __m512i key;
    NT* child;
    VT val;
    uint8_t offset;
    bool isVal;
};

//Architecture:
//Cuckoo hash tables for resolving collisions
//Lock on each individual entry (with the level you need) using the StripedLockTable, giving each actual table one locktable to make sure that deadlocks do not happen.
//Problem with this currently: using just size two StripedLockTable can cause deadlock, since we do HOH locking for the cuckoo insert step.
//Actually not a problem: we will not do proper hand over hand locking, since we are never deleting entries (just changing their values).
//Therefore, we can release the "parent" lock before getting the "child" lock, so the HOH step takes up only two locks total, not three.
//Still a potential locking problem: we have a fixed order of locking on queries--first, then second, which is fine if we just do queries.
//However, fixing the order for inserts is not viable---the whole structure of cuckoo hashing is that you effectively bounce around.
//If we move from table one to table two, then table two to table one for a different key, that could definitely cause deadlock, even ignoring unintentional deadlock from hashlocks
//Solution: we increase the complexity of the locking structure even further!!
//We can readlock however we want without causing problems, so, if we want to writelock a second entry and we currently hold a writelock on the "wrong" first entry, what we can do is:
//Downgrade the writelock to a readlock!! Then we readlock the correct first entry, then we writelock it, and finally we reupgrade the original entry.
//Maybe there is an even better solution though, as this does have the overhead of having to writelock an additional time (which is relatively expensive), but, well, whatever lol (for now)
//But this solution really is unideal, really the whole only two locks thing actually, cause it might cause some unwanted issues, such as:
//A value that is actually in the hash table seeming to dissapear, since we have removed it from the old node but not put it in the new node yet.
//Solution? Use three locks, but make them readlocks, and then writelock in the correct order? No that doesn't work, since again the deadlock problem.
//A not-good but at least somewhat functional solution: have a buffer in every entry for what you evicted or what you want to write to it.
//Problem is that on queries you have to check twice the data, which might be tenable with prefetching but still idk.
//Solution idea: make the HOH order not actually going say from first hash table to second hash table, but rather something more intelligent.
//The goal: we have a consistent locking order that based on hash values or whatever chooses whether to first lock the second hash table entry or the first hash table entry.
//The problem: it is impossible (I think) without at least inspecting the entries to know what this order could be (as you could have more than two keys share an entry in one hash table, which means that just knowledge of the endpoints is not enough)..
//And we can't inspect the entries, cause that requires readlocking and might break lock order.
//Actually oops I sorted all of these problems out in the B tree implementation that I did. Maybe not an ideal solution, but we can do as there:
//Readlock in one order, writelock in the other order, and when readlocking fail(restart) if the node is writelocked.
//Anyways smth along those lines not 100% sure.

template<typename NT, typename VT>
class HashTable {
    private:
        std::atomic<size_t> curUtilization; //Inneficient. Lol maybe bring back the partitioned counter or write smth similar.
        ReadWriteMutex reconstructMutex; //typically readlocked; writelocked if you want to reconstruct the hash table if collisions are a problem.
        std::array<std::vector<HashBucket<NT, VT>>, 2> tables;
        StripedLockTable entryLocks;
    
    public:
        HashTable(size_t size);
};

#endif