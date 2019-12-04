#include <stdio.h>
#include <inttypes.h>
#include <pthread.h>

#define NUM_PRODUCER                4
#define NUM_CONSUMER                NUM_PRODUCER
#define NUM_THREADS                 (NUM_PRODUCER + NUM_CONSUMER)
#define NUM_ENQUEUE_PER_PRODUCER    10000000
#define NUM_DEQUEUE_PER_CONSUMER    NUM_ENQUEUE_PER_PRODUCER

bool flag_verification[NUM_PRODUCER * NUM_ENQUEUE_PER_PRODUCER];
void enqueue(int key);
int dequeue();

// -------- Queue with coarse-grained locking --------
// ------------------- Modify here -------------------
#define QUEUE_SIZE      1024

struct QueueNode {
    int key;
    int flag;
};

QueueNode queue[QUEUE_SIZE];
uint64_t front;
uint64_t rear;

void init_queue(void) {
    front = 0;
    rear = 0;
}

void enqueue(int key) {
    uint64_t ticket = __sync_fetch_and_add(&rear, 1);
    QueueNode& bucket = queue[ticket % QUEUE_SIZE];

    while (1) {
        // 아래처럼 flag를 새로 선언해서 안하고 아래 if문을 직접 bucket.flag으로
        // 직접 접근해서 체크하면 프로그램이 안꺼진다.
        // => 이유: 디큐어에서 봤을 때, 처음에 플래그를 홀수임을 체크했다. flag가 1이라고 체크함
        // => 그 뒤에 다른 쓰레드가 플래그를 보고 자신의 라운드 임을 확인하고 flag++를 실행했다.
        // => 그리고 나서 flag는 여전히 짝수임에도 불구하고 자신의 라운드를 체크하기 전 홀수라고 먼저 체크했기 떄문에
        // => 자신의 라운드라고 생각해서 flag를 홀수고 바꿔줬다. 그러므로 무한루프 발생 가능
        int flag = bucket.flag;
        if (flag % 2 == 1 || flag / 2 != ticket / QUEUE_SIZE) {
            // bucket is already fiiled or not my turn
            pthread_yield();
        } else {
            break;
        }
    }
    bucket.key = key;
    bucket.flag++;
}

int dequeue(void) {
    uint64_t ticket = __sync_fetch_and_add(&front, 1);
    QueueNode& bucket = queue[ticket % QUEUE_SIZE];

    while (1) {
        int flag = bucket.flag;
        if (flag % 2 == 0 || flag / 2 != ticket / QUEUE_SIZE) {
            // bucket is not empty or not my turn
            pthread_yield();
        } else {
            break;
        }
    }
    int ret_key = bucket.key;
    bucket.flag++;

    return ret_key;
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

