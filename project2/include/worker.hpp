#ifndef _WORKER_HPP
#define _WORKER_HPP

#include <common.hpp>
#include <pthread.h>
#include <database.hpp>
#include <vector>
#include <fstream>

enum class WorkResult { FINISHED, DEADLOCK_DETECTED, TERMINATED };

class WorkerThread {
private:
    int tid;
    int threadStatus; // 0: uncreated, 1: created, 2: ruunning
    Database* db;
    pthread_t thread;
    std::ofstream clog;

    struct RecordWithFlag {
        Record record;
        bool isUpdated;
    };

    std::vector<RecordWithFlag> records;

    void* threadEntryPoint(void* args);
    bool doWork();
    WorkResult workTransaction();
    // bool terminate();

    void prepare();
    Record readRecord(record_index_t index);
    void updateRecord(record_index_t index, record_value_t value);
    void unlockAll();
    void rollback();
public:
    WorkerThread(int tid, Database* db);
    ~WorkerThread();

    bool work();
    bool join();
};

#endif
