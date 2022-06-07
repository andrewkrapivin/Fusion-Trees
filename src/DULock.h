#include <inttypes.h>

//Lol probably gotta learn c++ better to understand how to do this. Or even just asm

typedef struct alignas(64) lock_elem {
    volatile uint64_t c; //don't do volatile, do atomic?
}lock_elem;

template<int numThreads>
class DULock {
    private:
        lock_elem read_locks[numThreads];
        lock_elem write_lock[numThreads];
    public:
        DULock();
        void lock();
        void lock_shared(uint8_t thread_id);
        void upgrade(uint8_t thread_id);
        bool try_lock();
        bool try_upgrade(uint8_t thread_id);
        bool try_partial_upgrade(uint8_t thread_id);
        void finish_partial_upgrade();
        void unlock_partial_upgrade();
        bool try_lock_shared();
        void unlock_shared();
        void unlock();
};