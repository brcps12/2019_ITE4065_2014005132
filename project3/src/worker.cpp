#include <thread>
#include <random>
#include <limits>
#include <worker.hpp>
#include <snapshot.hpp>

#include <stdio.h>

thread_local int WorkerThread::myTid = 0;

WorkerThread::WorkerThread(int tid, Snapshot<int32_t> * snapshot) {
    this->tid = tid;
    this->snapshot = snapshot;
    this->numExecutions = 0;
    this->workState = 0;
    this->th = std::thread(&WorkerThread::entryPoint, this, tid);

    while (this->workState == 0) {
        std::this_thread::yield();
    }
}

void WorkerThread::entryPoint(int tid) {
    WorkerThread::myTid = tid;
    this->workState = 1;

    while (this->workState == 1) {
        std::this_thread::yield();
    }

    this->doWork();
}

void WorkerThread::doWork() {
    while (this->workState == 2) {
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
    this->workState = 2;
}

void WorkerThread::stop() {
    this->workState = 3;
}

void WorkerThread::join() {
    this->th.join();
}

u_int64_t WorkerThread::getNumExecutions() {
    return this->numExecutions;
}

int WorkerThread::getThreadId() {
    return WorkerThread::myTid;
}
