#ifndef _WORKER_HPP
#define _WORKER_HPP

#include <thread>
#include <types.hpp>

template<typename T>
class Snapshot;

class WorkerThread {
private:
    int tid;
    volatile int threadStatus;
    u_int64_t numExecutions;
    Snapshot<int32_t> * snapshot;
    std::thread th;

    void entryPoint(int tid);
    void doWork();
    int32_t rand();

    static thread_local int myTid;
public:
    WorkerThread() {}

    WorkerThread(int tid, Snapshot<int32_t> * snapshot);

    void work();
    u_int64_t terminate();

    static int getThreadId();
};

#endif
