#include <iostream>
#include <thread>
#include <chrono>
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

    Snapshot snapshot(N);
    WorkerThread **threads = new WorkerThread*[N];

    for (int i = 0; i < N; i++) {
        threads[i] = new WorkerThread(i + 1, &snapshot);
    }
    
    for (int i = 0; i < N; i++) {
        threads[i]->work();
    }

    // while 1 minutes
    chrono::minutes dura(1);
    this_thread::sleep_for(dura);

    u_int64_t numUpdates = 0;
    for (int i = 0; i < N; i++) {
        numUpdates += threads[i]->terminate();
    }

    cout << "Total number of updates: " << numUpdates << endl;

    for (int i = 0; i < N; i++) {
        delete threads[i];
    }
    
    delete[] threads;
    
    return 0;
}