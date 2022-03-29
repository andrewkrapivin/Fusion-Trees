#include "UpgradeableMutex.h"

void UpgradeableMutex::upgrade() {
    uniq_lock.lock();
    sh_mutex.unlock_shared();
    sh_mutex.lock();
}

bool UpgradeableMutex::try_upgrade(bool shared_unlock_if_false /* = true */) {
    bool can_lock = uniq_lock.try_lock();
    if (can_lock) {
        sh_mutex.unlock_shared();
        sh_mutex.lock();
    }
    else if(shared_unlock_if_false) {
        sh_mutex.unlock_shared();
    }
    return can_lock;
}

void UpgradeableMutex::unlock() {
    uniq_lock.unlock();
    sh_mutex.unlock();
}

void UpgradeableMutex::lock_shared() {
    sh_mutex.lock_shared();
}

void UpgradeableMutex::unlock_shared() {
    sh_mutex.unlock_shared();
}