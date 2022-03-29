#ifndef UPGRADEABLE_MUTEX_H_INCLUDED
#define UPGRADEABLE_MUTEX_H_INCLUDED

#include <mutex>
#include <shared_mutex>
#include <iostream>
#include <ostream>

using namespace std;

class UpgradeableMutex {
    private:
        std::shared_mutex sh_mutex;
        // std::shared_mutex wait_mutex;
        std::mutex uniq_lock;
    public:
        void upgrade(ostream& fout);
        bool try_upgrade(ostream& fout, bool shared_unlock_if_false = true);
        bool try_partial_upgrade(ostream& fout, bool shared_unlock_if_false = true);
        void finish_partial_upgrade(ostream& fout);
        //Essentially just says well fully unlock this mutex starting from the state of a partial upgrade
        void unlock_partial_upgrade(ostream& fout);
        void lock_shared(ostream& fout);
        void unlock_shared(ostream& fout);
        void unlock(ostream& fout);
};

#endif