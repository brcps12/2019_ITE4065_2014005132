#include <stdio.h>
#include <inttypes.h>
#include <pthread.h>

#define NUM_PRODUCER                4
#define NUM_CONSUMER                NUM_PRODUCER
#define NUM_THREADS                 (NUM_PRODUCER + NUM_CONSUMER)
#define NUM_ENQUEUE_PER_PRODUCER    10000000
#define NUM_DEQUEUE_PER_CONSUMER    NUM_ENQUEUE_PER_PRODUCER
#define VCAS                        __sync_val_compare_and_swap
#define CAS                         __sync_bool_compare_and_swap

bool flag_verification[NUM_PRODUCER * NUM_ENQUEUE_PER_PRODUCER];
void enqueue(int key);
int dequeue();

// -------- Queue with coarse-grained locking --------
// ------------------- Modify here -------------------
struct QueueNode {
    int key;
    QueueNode* next;
    QueueNode(int key): key(key) {}
};

QueueNode* head;
QueueNode* tail;

void init_queue(void) {
    QueueNode* sentinel = new QueueNode(0);
    head = tail = sentinel;
}

void enqueue(int key) {
    QueueNode* node = new QueueNode(key);
    QueueNode* prev;
    QueueNode* curr;

    while (1) {
        curr = tail;
        if (CAS(&curr->next, NULL, node)) {
            break;
        }

        CAS(&tail, curr, curr->next);
    }

    CAS(&tail, curr, node);
}

int dequeue(void) {
    QueueNode* prev;
    QueueNode* curr;

    while(1) {
        prev = head;
        curr = prev->next;

        if (curr == NULL) {
            pthread_yield();
        } else {
            if (CAS(&head, prev, curr)) {
                break;
            }
        }
    }

    return curr->key;
}
// ------------------------------------------------

void* ProducerFunc(void* arg) {
    long tid = (long)arg;

    int key_enqueue = NUM_ENQUEUE_PER_PRODUCER * tid;
    for (int i = 0; i < NUM_ENQUEUE_PER_PRODUCER; i++) {
        enqueue(key_enqueue);
        key_enqueue++;
    }

    return NULL;
}

void* ConsumerFunc(void* arg) {
    for (int i = 0; i < NUM_DEQUEUE_PER_CONSUMER; i++) {
        int key_dequeue = dequeue();
        flag_verification[key_dequeue] = true;
    }

    return NULL;
}

int main(void) {
    pthread_t threads[NUM_THREADS];

    init_queue();

    for (long i = 0; i < NUM_THREADS; i++) {
        if (i < NUM_PRODUCER) {
            pthread_create(&threads[i], 0, ProducerFunc, (void**)i);
        } else {
            pthread_create(&threads[i], 0, ConsumerFunc, NULL);
        }
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // verify
    for (int i = 0; i < NUM_PRODUCER * NUM_ENQUEUE_PER_PRODUCER; i++) {
        if (flag_verification[i] == false) {
            printf("INCORRECT!\n");
            return 0;
        }
    }
    printf("CORRECT!\n");

    return 0;
}

