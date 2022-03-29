#ifndef UPGRADEABLE_MUTEX_H_INCLUDED
#define UPGRADEABLE_MUTEX_H_INCLUDED

#include <mutex>
#include <shared_mutex>

class UpgradeableMutex {
    private:
        std::shared_mutex sh_mutex;
        std::mutex uniq_lock;
    public:
        void upgrade();
        bool try_upgrade(bool shared_unlock_if_false = true);
        void lock_shared();
        void unlock_shared();
        void unlock();
};

#endif