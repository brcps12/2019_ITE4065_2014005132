#ifndef _GLOBALSTATE_HPP
#define _GLOBALSTATE_HPP

#include <locktable.hpp>

#ifdef VERBOSE
#include <functional>
#endif

class GlobalState {
private:
    static int N, R, E;
    static pthread_mutex_t mutex;
    static int currentOrder;
    static int deadlockCnt;
    static LockTable* lockTable;

#ifdef VERBOSE
    static pthread_mutex_t execMutex;
#endif

public:
    static void init(int N, int R, int E);

    static void destroy();

    static int getNumRecords();

    static int getNumWorkers();

    static int getLastOrder();

    static int getNextOrder();

    static void lock();

    static void unlock();

    static LockTable& getLockTable();

    static void incDeadlockCnt();

    static int getDeadlockCnt();

#ifdef VERBOSE
    static void singleExec(std::function<void(void)> func);
#endif
};

#endif
