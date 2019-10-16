#include <stdio.h>
#include <pthread.h>
#include <math.h>

#define NUM_THREAD  10

pthread_cond_t cond;
pthread_mutex_t mutexes[NUM_THREAD];
pthread_t threads[NUM_THREAD];
int thread_ret[NUM_THREAD];

int range_start;
int range_end;

bool IsPrime(int n) {
    if (n < 2) {
        return false;
    }

    // FIX: change 0 to 2
    for (int i = 2; i <= sqrt(n); i++) {
        if (n % i == 0) {
            return false;
        }
    }
    return true;
}

void* ThreadFunc(void* arg) {
    long tid = (long)arg;
    thread_ret[tid] = 0;

    while (1) {
        pthread_cond_wait(&cond, &mutexes[tid]);

        if (thread_ret[tid] == -2) break;
    
        // Split range for this thread
        int start = range_start + ((range_end - range_start + 1) / NUM_THREAD) * tid;
        int end = range_start + ((range_end - range_start + 1) / NUM_THREAD) * (tid+1);
        if (tid == NUM_THREAD - 1) {
            end = range_end + 1;
        }
        
        long cnt_prime = 0;
        for (int i = start; i < end; i++) {
            if (IsPrime(i)) {
                cnt_prime++;
            }
        }

        thread_ret[tid] = cnt_prime;
    }
    
    pthread_exit(NULL);
    return NULL;
}

int CreateWorkers() {
    // Create threads to work
    for (long i = 0; i < NUM_THREAD; i++) {
        pthread_mutex_init(&mutexes[i], NULL);
        if (pthread_create(&threads[i], 0, ThreadFunc, (void*)i) < 0) {
            return -1;
        }
    }

    return 0;
}

int DoWork() {
    pthread_cond_broadcast(&cond);
}

void WaitWorkers() {
    // Step1: spinning to wait threads
    bool finished;
    while (1) {
        finished = true;
        for (int i = 0; i < NUM_THREAD; i++) {
            if (thread_ret[i] == -1) {
                finished = false;
                break;
            }
        }

        if (finished) break;
    }
}

void ReleaseWorkers() {
    for (long i = 0; i < NUM_THREAD; i++) {
        thread_ret[i] = -2; // release signal
    }

    pthread_cond_broadcast(&cond);
    
    for (int i = 0; i < NUM_THREAD; i++) {
        pthread_join(threads[i], NULL);
        pthread_mutex_destroy(&mutexes[i]);
    }
}

int main(void) {
    pthread_cond_init(&cond, NULL);

    if(CreateWorkers() == -1) {
        printf("pthread_create error!\n");
        return 0;
    }

    WaitWorkers();

    int k = 0;
    while (1) {
        // Input range
        scanf("%d", &range_start);
        if (range_start == -1) {
            break;
        }
        scanf("%d", &range_end);

        // reset result for each threads
        for (long i = 0; i < NUM_THREAD; i++) {
            thread_ret[i] = -1;
        }

        DoWork();

        WaitWorkers();

        // Collect results
        int cnt_prime = 0;
        for (int i = 0; i < NUM_THREAD; i++) {
            cnt_prime += thread_ret[i];
        }
        printf("%d: number of prime: %d\n", ++k, cnt_prime);
    }

    // Wait threads end
    ReleaseWorkers();

    pthread_cond_destroy(&cond);
 
    return 0;
}

