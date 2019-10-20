#include <common.hpp>

static bool isKeyInitialized = false;
static pthread_key_t WORKERTHREAD_TID_KEY;

void setThreadId(int* tid) {
    if (!isKeyInitialized) {
        pthread_key_create(&WORKERTHREAD_TID_KEY, NULL);
        isKeyInitialized = true;
    }

    pthread_setspecific(WORKERTHREAD_TID_KEY, tid);
}

int getThreadId() {
    void* ptr = pthread_getspecific(WORKERTHREAD_TID_KEY);

    if (ptr == NULL) return -1;

    return *((int*)ptr);
}
