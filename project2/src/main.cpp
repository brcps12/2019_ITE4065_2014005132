#include <iostream>
#include <string>
#include <common.hpp>
#include <globalstate.hpp>
#include <database.hpp>
#include <worker.hpp>

using namespace std;

int main(int argc, char *argv[]) {
    if (argc < 4) {
        cout << "usage: " << argv[0] << " <N> <R> <E>" << endl;
        cout << "N" << "\tthe number of worker threads" << endl;
        cout << "R" << "\tthe number of records" << endl;
        cout << "E" << "\tlast global execution order" << endl;

        return 0;
    }

    int N = atoi(argv[1]), R = atoi(argv[2]), E = atoi(argv[3]);

    cout << "The number of worker threads <N>: " << N << endl;
    cout << "The number of records <R>: " << E << endl;
    cout << "The number of transactions <E>: " << E << endl;

    // initialize database
    Database *db = new Database(R);

#ifdef VERBOSE
    cout << "Database with " << R << " records is created" << endl;
    cout << "All values of records are " << DEFAULT_RECORD_VALUE << endl;
#endif

    // initialize workers
    GlobalState::init(N, R, E);

    WorkerThread **threads = new WorkerThread*[N];

    // create worker threads
    for (int i = 0; i < N; i++) {
        threads[i] = new WorkerThread(i + 1, db);

#ifdef VERBOSE
        cout << "Thread #" << (i + 1) << " is created" << endl;
#endif
    }

#ifdef VERBOSE
        cout << N << " worker thread(s) creation complete" << endl;
#endif

    // execute transactions
    for (int i = 0; i < N; i++) {
        threads[i]->work();
    }

    // join them all
    for (int i = 0; i < N; i++) {
        threads[i]->join();
    }

#ifdef VERBOSE
        cout << "All worker threads are finished" << endl;
#endif

    cout << "Total " << GlobalState::getDeadlockCnt() << " Deadlock(s) occur" << endl;

    // clean resources
    GlobalState::destroy();

    delete[] threads;
    delete db;

    return 0;
}