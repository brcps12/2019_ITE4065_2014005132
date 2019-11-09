#include <thread>
#include <random>
#include <climits>
#include <worker.hpp>
#include <snapshot.hpp>

thread_local int WorkerThread::tid = 0;

WorkerThread::WorkerThread(int tid, Snapshot* snapshot) {
    this->snapshot = snapshot;
    this->numExecutions = 0;
    this->threadStatus = 0;
    this->thread = std::thread(&WorkerThread::entryPoint, this, tid);

    while (this->threadStatus == 0) {
        std::this_thread::yield();
    }
}

void WorkerThread::entryPoint(int tid) {
    WorkerThread::tid = tid;
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

int WorkerThread::rand() {
    thread_local std::mt19937 engine(std::random_device{}());
    std::uniform_int_distribution<int> dist(INT_MIN, INT_MAX);

    return dist(engine);
}

void WorkerThread::work() {
    this->threadStatus = 2;
}

u_int64_t WorkerThread::terminate() {
    this->threadStatus = 3;
    this->thread.join();
    return this->numExecutions;
}

int WorkerThread::currentThreadId() {
    return WorkerThread::tid;
}
