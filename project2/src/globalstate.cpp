#include <globalstate.hpp>
#include <pthread.h>

int GlobalState::N, GlobalState::R, GlobalState::E;
pthread_mutex_t GlobalState::mutex;
int GlobalState::currentOrder = 0;
int GlobalState::deadlockCnt = 0;
LockTable* GlobalState::lockTable;

#ifdef VERBOSE
pthread_mutex_t GlobalState::execMutex;
#endif

void GlobalState::init(int N, int R, int E) {
    GlobalState::N = N;
    GlobalState::R = R;
    GlobalState::E = E;
    pthread_mutex_init(&GlobalState::mutex, NULL);
    lockTable = new LockTable(N, R);

#ifdef VERBOSE
    pthread_mutex_init(&GlobalState::execMutex, NULL);
#endif
}

void  GlobalState::destroy() {
    pthread_mutex_destroy(&GlobalState::mutex);
    delete lockTable;
}

int GlobalState::getNumRecords() {
    return R;
}

int GlobalState::getNumWorkers() {
    return N;
}

int GlobalState::getLastOrder() {
    return E;
}

int GlobalState::getNextOrder() {
    return __sync_add_and_fetch(&currentOrder, 1);
}

void GlobalState::lock() {
    pthread_mutex_lock(&GlobalState::mutex);
}

void GlobalState::unlock() {
    pthread_mutex_unlock(&GlobalState::mutex);
}

LockTable& GlobalState::getLockTable() {
    return *lockTable;
}

void GlobalState::incDeadlockCnt() {
    __sync_fetch_and_add(&GlobalState::deadlockCnt, 1);
}

int GlobalState::getDeadlockCnt() {
    return GlobalState::deadlockCnt;
}

#ifdef VERBOSE
void GlobalState::singleExec(std::function<void(void)> func) {
    pthread_mutex_lock(&GlobalState::execMutex);
    func();
    pthread_mutex_unlock(&GlobalState::execMutex);
}
#endif
