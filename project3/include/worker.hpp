#ifndef _WORKER_HPP
#define _WORKER_HPP

#include <thread>
#include <sys/types.h>
#include <snapshot.hpp>

class WorkerThread {
private:
    std::thread thread;
    Snapshot * snapshot;
    volatile int threadStatus;
    u_int64_t numExecutions;

    void entryPoint(int tid);

    void doWork();

    int rand();

    static thread_local int tid;
public:
    WorkerThread() {}

    WorkerThread(int tid, Snapshot * snapshot);

    void work();

    u_int64_t terminate();

    static int currentThreadId();
};

#endif
