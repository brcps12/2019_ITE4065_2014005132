#include <thread>
#include <random>
#include <limits>
#include <worker.hpp>
#include <snapshot.hpp>

thread_local int WorkerThread::myTid = 0;

WorkerThread::WorkerThread(int tid, Snapshot<int32_t> * snapshot) {
    this->tid = tid;
    this->snapshot = snapshot;
    this->numExecutions = 0;
    this->threadStatus = 0;
    this->th = std::thread(&WorkerThread::entryPoint, this, tid);

    while (this->threadStatus == 0) {
        std::this_thread::yield();
    }
}

void WorkerThread::entryPoint(int tid) {
    WorkerThread::myTid = tid;
    this->threadStatus = 1;

    while (this->threadStatus == 1) {
        std::this_thread::yield();
    }

    this->doWork();
}

void WorkerThread::doWork() {
    while (this->threadStatus == 2) {
        this->snapshot->update(rand());
        ++this->numExecutions;
    }
}

int32_t WorkerThread::rand() {
    thread_local std::mt19937 engine(std::random_device{}());
    std::uniform_int_distribution<int32_t> dist(INT32_MIN, INT32_MAX);

    return dist(engine);
}

void WorkerThread::work() {
    this->threadStatus = 2;
}

u_int64_t WorkerThread::terminate() {
    this->threadStatus = 3;
    this->th.join();
    return this->numExecutions;
}

int WorkerThread::getThreadId() {
    return WorkerThread::myTid;
}
