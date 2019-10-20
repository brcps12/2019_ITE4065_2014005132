#include <iostream>
#include <common.hpp>
#include <worker.hpp>
#include <util.hpp>
#include <database.hpp>
#include <locktable.hpp>
#include <globalstate.hpp>

typedef void * (*THREADFUNCPTR)(void *);

WorkerThread::WorkerThread(int tid, Database* db) {
    char filename[30];
    sprintf(filename, "thread%d.txt", tid);
    this->tid = tid;
    this->db = db;
    this->threadStatus = 0;
    this->clog.open(filename);
    pthread_create(&thread, 0, (THREADFUNCPTR)&WorkerThread::threadEntryPoint, this);

    while (this->threadStatus == 0) {
        pthread_yield();
    }
}

WorkerThread::~WorkerThread() {
    this->clog.close();
}

void* WorkerThread::threadEntryPoint(void* args) {
    setThreadId(&this->tid);
    this->threadStatus = 1;

    while (this->threadStatus == 1) {
        pthread_yield();
    }

    this->doWork();

    return (void *)NULL;
}

bool WorkerThread::work() {
    this->threadStatus = 2;
    return true;
}

bool WorkerThread::doWork() {
    while (true) {
        WorkResult result = workTransaction();

        if (result == WorkResult::TERMINATED) {
#ifdef VERBOSE
            GlobalState::singleExec([&]() {
                std::cout << "[Tid:" << tid << "] >> " << "Terminated" << std::endl;
            });
#endif
            break;
        }

        if (result == WorkResult::DEADLOCK_DETECTED) {
            GlobalState::incDeadlockCnt();
#ifdef VERBOSE
            GlobalState::singleExec([&]() {
                std::cout << "[Tid:" << tid << "] >> " << "Deadlock detected! Rollback and restart" << std::endl;
            });
#endif
        }
    }

    return true;
}

bool WorkerThread::join() {
    return pthread_join(thread, NULL) == 0;
}

void WorkerThread::prepare() {
    this->records.clear();
}

Record WorkerThread::readRecord(record_index_t index) {
    Record record = this->db->get(index);
    this->records.push_back({ record, false });
    return record.clone();
}

void WorkerThread::updateRecord(record_index_t index, record_value_t value) {
    int idx = -1;
    for (int i = 0; i < records.size(); i++) {
        if (records[i].record.index == index) {
            idx = i;
            break;
        }
    }

    if (idx == -1) {
        idx = records.size();
        readRecord(index);
    }

    records[idx].isUpdated = true;
    this->db->set(index, value);
}

void WorkerThread::unlockAll() {
    GlobalState::getLockTable().unlockAll();
}

void WorkerThread::rollback() {
    for (int i = 0; i < records.size(); i++) {
        if (records[i].isUpdated) {
            this->db->set(records[i].record.index, records[i].record.value);
        }
    }
}

WorkResult WorkerThread::workTransaction() {
    // randomly pick
#ifdef VERBOSE
    GlobalState::singleExec([&]() {
        std::cout << "[Tid:" << tid << "] >> " << "Begins transaction" << std::endl;
    });
#endif

    this->prepare();

    int i, j, k;
    do {
        i = randomNumber(1, GlobalState::getNumRecords());
        j = randomNumber(1, GlobalState::getNumRecords());
        k = randomNumber(1, GlobalState::getNumRecords());
    } while (i == j || j == k || k == i);

#ifdef VERBOSE
    GlobalState::singleExec([&]() {
        std::cout << "[Tid:" << tid << "] >> " << "Pick i, j, k: " << i << ", " << j << ", " << k << " respectively" << std::endl;
    });
#endif

    // Read Ri
    GlobalState::lock();

#ifdef VERBOSE
    GlobalState::singleExec([&]() {
        std::cout << "[Tid:" << tid << "] >> " << "Try to acquire reader lock for record i(" << i << ")" << std::endl;
    });
#endif

    if (GlobalState::getLockTable().rdLock(i) == AcquiringLockResult::DEADLOCK_DETECTED) {
        this->rollback();
        this->unlockAll();

#ifdef VERBOSE
        GlobalState::singleExec([&]() {
            std::cout << "[Tid:" << tid << "] >> " << "Acquiring lock for record i(" << i << ") failed!: Deadlock detected" << std::endl;
        });
#endif

        GlobalState::unlock();
        return WorkResult::DEADLOCK_DETECTED;
    }

#ifdef VERBOSE
    GlobalState::singleExec([&]() {
        std::cout << "[Tid:" << tid << "] >> " << "Successfully acquired lock for record i(" << i << ")" << std::endl;
    });
#endif

    GlobalState::unlock();

    Record ri = this->readRecord(i);

#ifdef VERBOSE
    GlobalState::singleExec([&]() {
        std::cout << "[Tid:" << tid << "] >> " << "Read record i: " << i << ", value: " << ri.value << std::endl;
    });
#endif

    // Read Rj and update
    GlobalState::lock();

#ifdef VERBOSE
    GlobalState::singleExec([&]() {
        std::cout << "[Tid:" << tid << "] >> " << "Try to acquire writer lock for record j(" << j << ")" << std::endl;
    });
#endif

    if (GlobalState::getLockTable().wrLock(j) == AcquiringLockResult::DEADLOCK_DETECTED) {
        this->rollback();
        this->unlockAll();

#ifdef VERBOSE
        GlobalState::singleExec([&]() {
            std::cout << "[Tid:" << tid << "] >> " << "Acquiring lock for record j(" << j << ") failed!: Deadlock detected" << std::endl;
        });
#endif

        GlobalState::unlock();
        return WorkResult::DEADLOCK_DETECTED;
    }

#ifdef VERBOSE
    GlobalState::singleExec([&]() {
        std::cout << "[Tid:" << tid << "] >> " << "Successfully acquired lock for record j(" << j << ")" << std::endl;
    });
#endif

    GlobalState::unlock();

    Record rj = this->readRecord(j);
    rj.value += ri.value + 1;
    this->updateRecord(j, rj.value);

#ifdef VERBOSE
    GlobalState::singleExec([&]() {
        std::cout << "[Tid:" << tid << "] >> " << "Update record j: " << j << ", value: " << rj.value << std::endl;
    });
#endif

    // Read Rk and update
    GlobalState::lock();

#ifdef VERBOSE
    GlobalState::singleExec([&]() {
        std::cout << "[Tid:" << tid << "] >> " << "Try to acquire writer lock for record k(" << k << ")" << std::endl;
    });
#endif

    if (GlobalState::getLockTable().wrLock(k) == AcquiringLockResult::DEADLOCK_DETECTED) {
        this->rollback();
        this->unlockAll();

#ifdef VERBOSE
    GlobalState::singleExec([&]() {
        std::cout << "[Tid:" << tid << "] >> " << "Acquiring lock for record k(" << k << ") failed!: Deadlock detected" << std::endl;
    });
#endif

        GlobalState::unlock();
        return WorkResult::DEADLOCK_DETECTED;
    }
#ifdef VERBOSE
    GlobalState::singleExec([&]() {
        std::cout << "[Tid:" << tid << "] >> " << "Successfully acquired lock for record k(" << k << ")" << std::endl;
    });
#endif

    GlobalState::unlock();

    Record rk = this->readRecord(k);
    rk.value -= ri.value;
    this->updateRecord(k, rk.value);

#ifdef VERBOSE
    GlobalState::singleExec([&]() {
        std::cout << "[Tid:" << tid << "] >> " << "Update record k: " << k << ", value: " << rk.value << std::endl;
    });
#endif

    // Commit phase
    GlobalState::lock();
    int commitId = GlobalState::getNextOrder();

#ifdef VERBOSE
    GlobalState::singleExec([&]() {
        std::cout << "[Tid:" << tid << "] >> " << "Commit phase, commitId: " << commitId << std::endl;
    });
#endif

    if (commitId > GlobalState::getLastOrder()) {
        this->rollback();
    }

    this->unlockAll();

    if (commitId > GlobalState::getLastOrder()) {
        GlobalState::unlock();
        return WorkResult::TERMINATED;
    }

    // append commit log
    // ...
    clog << commitId << " " << i << " " << j << " " << k << " " << ri.value << " " << rj.value << " " << rk.value << std::endl;


#ifdef VERBOSE
    GlobalState::singleExec([&]() {
        std::cout << "[Tid:" << tid << "] >> " << "Commited!" << std::endl;
    });
#endif

    GlobalState::unlock();
    return WorkResult::FINISHED;
}

