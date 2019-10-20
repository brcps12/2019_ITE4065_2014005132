#ifndef _LOCKTABLE_HPP
#define _LOCKTABLE_HPP

#include <sys/types.h>
#include <set>
#include <list>
#include <vector>
#include <database.hpp>

#ifdef VERBOSE
#include <stdio.h>
#endif

enum class AcquiringLockResult { ACQUIRED, DEADLOCK_DETECTED };

struct LockElement {
    int tid;
    bool isReader;
    bool isBlocked;
};

struct LockToken {
    int tid;
    record_index_t index;
    std::list<LockElement>::iterator lock;
};

class RWLock {
private:
    record_index_t index;
    std::list<LockElement> threadQueue;
    int rdCnt, wrCnt;

    LockToken addLock(bool isReader);
public:
    RWLock() {}

    RWLock(record_index_t index) {
        this->index = index;
    }
    LockToken addRdLock();
    LockToken addWrLock();
    void unlock(LockToken& token);
    bool canEnter(LockToken& token);
    bool isFirst(LockToken& token) {
        return token.lock == this->threadQueue.begin();
    }
    void setIndex(record_index_t index) {
        this->index = index;
    }

#ifdef VERBOSE
    void printTids() {
        for (auto it = threadQueue.begin(); it != threadQueue.end(); it++) {
            printf("<- [%c(%d,%s)] ", it->isReader ? 'r' : 'w', it->tid, it->isBlocked ? "b" : "p");
        }
    }
#endif
};

class LockTable {
private:
    int numWorkers, numRecords;
    std::vector<RWLock> locks;
    std::vector<std::vector<LockToken> > adj;

    AcquiringLockResult rwlock(record_index_t index, bool isReader);
    bool hasCycle(LockToken& token);
    bool detectCycle(int nextTid, bool blocked, std::set<int>& visit);
public:
    LockTable(int numWorkers, int numRecords) {
        this->numWorkers = numWorkers;
        this->numRecords = numRecords;
        this->adj.resize(numWorkers);
        for (int i = 0; i < numRecords; i++) {
            this->locks.push_back(RWLock(i + 1));
        }
    }

    ~LockTable() {
    }

    AcquiringLockResult rdLock(record_index_t index);
    AcquiringLockResult wrLock(record_index_t index);
    void unlockAll();

#ifdef VERBOSE
    void printLockTable() {
        printf("<<<LockTable>>>\n");
        for (int i = 0; i < numRecords; i++) {
            printf("Record %d: ", i + 1);
            locks[i].printTids();
            printf("\n");
        }
    }
#endif
};

#endif
