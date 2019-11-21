#include <stdio.h>
#include <pthread.h>

#define NUM_THREAD      16
#define NUM_WORK        1000000

int cnt_global;
/* to allocate cnt_global & object_tas in different cache lines */
// 같은 캐시라인에 있게 되면 CPU1에서 cnt_global을 수정하고 CPU2에서는 변경하지 않고 있지만 cnt_global이 수정되어 cache invalidate가 발생한다.
// 따라서 CPU2에서는 사용하지도 않은 cnt_global 때문에 다시 fetch하는 경우가 발생한다
// 그러므로 두 변수는 다른 캐시라인에 두어 서로에 의해 다시 fetch하는 일이 없도록 방지한다.
// most architecture set size of cache line to 64bytes
int gap[128];
int object_tas;

void lock(int *lock_object) {
    while (__sync_lock_test_and_set(lock_object, 1) == 1) {}
}

void unlock(int* lock_object) {
    // critical section 이후에 실행되도록 보장하기 위함
    __sync_synchronize();
    *lock_object = 0;
}

void* thread_work(void* args) {
    for (int i = 0; i < NUM_WORK; i++) {
        lock(&object_tas);
        cnt_global++;
        unlock(&object_tas);
    }
}

int main(void) {
    pthread_t threads[NUM_THREAD];

    for (int i = 0; i < NUM_THREAD; i++) {
        pthread_create(&threads[i], NULL, thread_work, NULL);
    }
    for (int i = 0; i < NUM_THREAD; i++) {
        pthread_join(threads[i], NULL);
    }
    printf("cnt_global: %d\n", cnt_global);

    return 0;
}
