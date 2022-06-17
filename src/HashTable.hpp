#ifndef HASHTABLE_INCLUDED
#define HASHTABLE_INCLUDED

constexpr size_t MLPSize = 8; //number cachelines prefetchable at once. Not sure what this should actually be
constexpr size_t EntriesPerBucket = 4;

typedef uint64_t HashType;
typedef uint32_t SHashType;

template<typename NT>
struct HashBucket alignas(64) {
    // std::shared_mutex mtx;
    uint8_t offsets[EntriesPerBucket]; //4 bytes
    // uint32_t shashSeed; //4 bytes
    SHashType shashes[EntriesPerBucket]; //16 bytes
    NT* nodeptrs[EntriesPerBucket]; //32 bytes
    //Total: 56 bytes-- (almost) one cache line for four entries
};

constexpr numEachSide = (MLPSize/2 - 1) * 64 / (EntriesPerBucket * (sizeof(HashType) + sizeof(SHashType)));

template<typename NT>
struct MLPHashBucket alignas(64) {
    HashBucket<NT> b;
    HashType successorHashes[EntriesPerBucket][numEachSide];
    SHashType successorSHashes[EntriesPerBucket][numEachSide];
    HashType predecessorHashes[EntriesPerBucket][numEachSide];
    SHashType predecessorSHashes[EntriesPerBucket][numEachSide];
};

template<typename NT>
class HashTable {
    private:
        size_t size;
        vector<HashBucket> table;
    
    public:

};

#endif