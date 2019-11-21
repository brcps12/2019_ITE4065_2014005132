#include <stdio.h>
#include <pthread.h>

#define NUM_THREAD      4
#define NUM_WORK        1000000

int cnt_global;
/* to allocate cnt_global & object_tas in different cache lines */
int gap[128];
int nodes[NUM_THREAD + 1];
int *tail = &nodes[0];

void lock(int** node, int** pred) {
    **node = 1;
    *pred = __sync_lock_test_and_set(&tail, *node);
    while (**pred == 1) {
        // pthread_yield();
    }
}

void unlock(int** node, int** pred) {
    __sync_synchronize();
    **node = 0;
    *node = *pred;
}

void* thread_work(void* args) {
    int *node = (int*)args, *pred = NULL;

    for (int i = 0; i < NUM_WORK; i++) {
        lock(&node, &pred);
        cnt_global++;
        unlock(&node, &pred);
    }
}

int main(void) {
    pthread_t threads[NUM_THREAD];

    nodes[0] = 0;
    for (int i = 0; i < NUM_THREAD; i++) {
        pthread_create(&threads[i], NULL, thread_work, &nodes[i + 1]);
    }
    for (int i = 0; i < NUM_THREAD; i++) {
        pthread_join(threads[i], NULL);
    }
    printf("cnt_global: %d\n", cnt_global);

    return 0;
}
