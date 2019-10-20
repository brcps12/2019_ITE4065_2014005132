#include <set>
#include <assert.h>
#include <common.hpp>
#include <locktable.hpp>
#include <globalstate.hpp>

LockToken RWLock::addLock(bool isReader) {
    int tid = getThreadId();
    LockElement elem = { tid, isReader, true };

    threadQueue.push_back(elem);
    auto lockElem = threadQueue.end();
    --lockElem;
    return { tid, index, lockElem };
}

LockToken RWLock::addRdLock() {
    return this->addLock(true);
}

LockToken RWLock::addWrLock() {
    return this->addLock(false);
}

void RWLock::unlock(LockToken& token) {
    threadQueue.erase(token.lock);
}

bool RWLock::canEnter(LockToken& token) {
    auto tmp = token.lock;
    if (token.lock->isBlocked == false || token.lock == threadQueue.begin()) {
        token.lock->isBlocked = false;
        return true;
    }

    --tmp;
    if (token.lock->isReader) {
        if (tmp->isReader && tmp->isBlocked == false) {
            token.lock->isBlocked = false;
            return true;
        }
    }

    return false;
}

AcquiringLockResult LockTable::rwlock(record_index_t index, bool isReader) {
    int tid = getThreadId();
    RWLock& lock = this->locks[index - 1];
    LockToken token = isReader ? lock.addRdLock() : lock.addWrLock();
    this->adj[tid - 1].push_back(token);

    if (hasCycle(token)) {
        return AcquiringLockResult::DEADLOCK_DETECTED;
    }

    // waiting
    while (!locks[index - 1].canEnter(token)) {
        GlobalState::unlock();
        pthread_yield();
        GlobalState::lock();
    }

    return AcquiringLockResult::ACQUIRED;
}

AcquiringLockResult LockTable::rdLock(record_index_t index) {
    return rwlock(index, true);
}

AcquiringLockResult LockTable::wrLock(record_index_t index) {
    return rwlock(index, false);
}

void LockTable::unlockAll() {
    int tid = getThreadId();
    for (LockToken& token : adj[tid - 1]) {
        this->locks[token.index - 1].unlock(token);
    }
    adj[tid - 1].clear();
}

bool LockTable::hasCycle(LockToken& token) {
    if (locks[token.index - 1].canEnter(token)) return false;

#ifdef VERBOSE
    printLockTable();
#endif

    auto it = token.lock;
    --it;
    if (it->tid == token.tid) {
        // this situation never occurs
        assert(false);
        return true;
    }

    std::set<int> visit;

    return detectCycle(it->tid, false, visit);
}

bool LockTable::detectCycle(int nextTid, bool blocked, std::set<int>& visit) {
    if (nextTid == getThreadId()) return blocked;

    visit.insert(nextTid);

    for (LockToken& token : adj[nextTid - 1]) {
        if (locks[token.index - 1].isFirst(token)) {
            continue;
        }

        auto it = token.lock;
        --it;

        if (visit.find(it->tid) == visit.end()) {
            if (detectCycle(it->tid, blocked | token.lock->isBlocked, visit)) {
                return true;
            }
        }
    }

    return false;
}
