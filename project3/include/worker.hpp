#ifndef _WORKER_HPP
#define _WORKER_HPP

#include <thread>
#include <types.hpp>

template<typename T>
class Snapshot;

class WorkerThread {
private:
    int tid;
    int workState;
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
    void stop();
    void join();
    u_int64_t getNumExecutions();

    static int getThreadId();
};

#endif
