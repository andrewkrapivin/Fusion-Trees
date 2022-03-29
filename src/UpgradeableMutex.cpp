#include "UpgradeableMutex.h"
#include <iostream>

void UpgradeableMutex::upgrade(ostream& fout) {
    // fout << "Locking" << std::endl;
    uniq_lock.lock();
    // wait_mutex.lock();
    sh_mutex.unlock_shared();
    sh_mutex.lock();
    // wait_mutex.unlock();
    // fout << "Locked" << std::endl;
}

bool UpgradeableMutex::try_upgrade(ostream& fout, bool shared_unlock_if_false /* = true */) {
    // fout << "Trying to lock" << std::endl;
    bool can_lock = uniq_lock.try_lock();
    if (can_lock) {
        // fout << "Succeeded first part of locking" << std::endl;
        // wait_mutex.lock();
        sh_mutex.unlock_shared();
        sh_mutex.lock();
        // wait_mutex.unlock();
        // fout << "Succeeded at locking!" << std::endl;
    }
    else if(shared_unlock_if_false) {
        // fout <<"Failed to lock" << std::endl;
        sh_mutex.unlock_shared();
    }
    return can_lock;
}

bool UpgradeableMutex::try_partial_upgrade(ostream& fout, bool shared_unlock_if_false /* = true */) {
    // fout << "Trying to partial lock" << std::endl;
    bool can_lock = uniq_lock.try_lock();
    if(can_lock) {
        sh_mutex.unlock_shared(); //the lock on the uniq_lock acts as if its a read lock for now! Since no one will write until they get it!
    }
    else if(!can_lock && shared_unlock_if_false) {
        // fout <<"Failed to lock" << std::endl;
        sh_mutex.unlock_shared();
    }
    return can_lock;
}

void UpgradeableMutex::finish_partial_upgrade(ostream& fout) {
    // fout << "Trying to finish partial upgrade" << std::endl;
    sh_mutex.lock();
    // fout << "Finished partial upgrade" << std::endl;
}

void UpgradeableMutex::unlock_partial_upgrade(ostream& fout) {
    // fout << "Unlocked partial upgrade" << std::endl;
    uniq_lock.unlock();
}

void UpgradeableMutex::unlock(ostream& fout) {
    // fout << "Unlocked" << std::endl;
    uniq_lock.unlock();
    sh_mutex.unlock();
}

void UpgradeableMutex::lock_shared(ostream& fout) {
    // wait_mutex.lock_shared();
    // fout << "Trying to lock shared" << std::endl;
    sh_mutex.lock_shared();
    // fout << "Locked shared" << std::endl;
    // wait_mutex.unlock_shared();
}

void UpgradeableMutex::unlock_shared(ostream& fout) {
    // fout << "Unlocked shared" << std::endl;
    sh_mutex.unlock_shared();
}