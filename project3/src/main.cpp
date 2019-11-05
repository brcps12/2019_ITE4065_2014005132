#include <iostream>
#include <thread>
#include <chrono>
#include <types.hpp>
#include <snapshot.hpp>
#include <worker.hpp>

using namespace std;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        cout << "usage: " << argv[0] << " <N>" << endl;
        cout << "N" << "\tthe number of worker threads" << endl;

        return 0;
    }

    int N = atoi(argv[1]);

    cout << "The number of worker threads <N>: " << N << endl;

    Snapshot<int32_t> snapshot(N, 0);
    Array<WorkerThread*> threads(N);

    for (int tid = 1; tid <= N; tid++) {
        threads[tid - 1] = new WorkerThread(tid, &snapshot);
    }
    
    for (int i = 0; i < N; i++) {
        threads[i]->work();
    }

    // while 1 minutes
    chrono::minutes dura(1);
    this_thread::sleep_for(dura);

    u_int64_t numExecutions = 0;
    for (int i = 0; i < N; i++) {
        numExecutions += threads[i]->getNumExecutions();
        threads[i]->stop();
    }

    for (int i = 0; i < N; i++) {
        threads[i]->join();
    }

    cout << "Total number of updates: " << numExecutions << endl;

    for (int i = 0; i < N; i++) {
        delete threads[i];
    }
    
    return 0;
}