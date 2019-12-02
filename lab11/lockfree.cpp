#include <stdio.h>
#include <limits.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

#define MAX_NUM_THREAD  16
#define MAX_NUM_WORK    100000

#define ISMARKED(p)     (((unsigned long long)p & 0x8000000000000000LL) != 0ULL)
#define MARKED(p)       ((Node*)((unsigned long long)p | 0x8000000000000000ULL))
#define UNMARKED(p)     ((Node*)((unsigned long long)p & 0x7FFFFFFFFFFFFFFFULL))
#define REFERENCE(p)    ((Node*)UNMARKED(p))
#define CAS(p, a, b)    __sync_bool_compare_and_swap(p, a, b)

// list node structure
struct Node {
    int key;
    Node* next;
};

// linked list
Node* head;
Node* tail;

// workload data
struct Work {
    char type;
    int value;
};

Work thread_work[MAX_NUM_THREAD][MAX_NUM_WORK];
int num_work;

// initialize linked list
void list_init(Node** head, Node** tail) {
    *head = (Node*)malloc(sizeof(Node));
    (*head)->key = INT_MIN;
    *tail = (Node*)malloc(sizeof(Node));
    (*tail)->key = INT_MAX;

    (*head)->next = *tail;
    (*tail)->next = NULL;
}

void list_find(int key, Node** outPrev, Node** outCurr) {
    Node* prev;
    Node* curr;
    Node* succ;

RETRY:
    prev = head;
    curr = head->next;
        
    while(1) {
        // REFERENCE 매크로를 사용하는 위치를 파악해서
        // 어느 곳이 unmarked가 보장되는지를 알아보자.
        // 중간에 prev가 지워지는 경우, curr가 지워지는 경우, succ가 지워지는 경우 모두
        // 생각해 보면서 그려보자.
        succ = curr->next;
        while (ISMARKED(succ)) {
            if (!CAS(&prev->next, curr, UNMARKED(succ))) {
                goto RETRY;
            }
            curr = REFERENCE(succ);
            succ = curr->next;
        }

        if (curr->key >= key) {
            *outPrev = prev, *outCurr = curr;
            return;
        }
        prev = curr;
        curr = succ;
    }
}

// add key into the list
bool list_add(int key) {
    Node* prev;
    Node* curr;
    Node* node = (Node*)malloc(sizeof(Node));
    node->key = key;

    do {
        list_find(key, &prev, &curr);

        if (curr->key == key) {
            free(node);
            return false;
        }

        node->next = UNMARKED(curr);
    } while(!CAS(&prev->next, UNMARKED(curr), node));
    
    return true;
}

// remove key from the list
bool list_remove(int key) {
    Node* prev;
    Node* curr;
    Node* succ;

    while (1) {
        list_find(key, &prev, &curr);

        if (curr->key != key) {
            return false;
        }

        succ = UNMARKED(curr->next);
        if (!CAS(&curr->next, succ, MARKED(succ))) {
            continue;
        }
    }

    // 이 CAS가 실패해도 상관이 없다. 왜그럴까? (생각해보기)
    // ...
    CAS(&prev->next, UNMARKED(curr), succ);
    return true;
}

// check whether a key is in the list
bool list_contains(int key) {
    Node* curr = head;
    while (curr->key < key) {
        curr = REFERENCE(curr->next);
    }

    return curr->key == key && !ISMARKED(curr->next);
}

// do workloads for each threads
void* ThreadFunc(void* args) {
    long tid = (long)args;

    for (int i = 0; i < num_work; i++) {
        switch (thread_work[tid][i].type) {
            case 'A':
                list_add(thread_work[tid][i].value);
                break;
            case 'R':
                list_remove(thread_work[tid][i].value);
                break;
            case 'C':
                list_contains(thread_work[tid][i].value);
                break;
            default:
                break;
        }
    }

    return NULL;
}

int main(void) {
    int num_thread;

    // input workloads
    FILE* fp = fopen("workload.txt", "r");

    fscanf(fp, "%d %d", &num_thread, &num_work);

    int thread_num;
    char work_type;
    int value;
    for (int i = 0; i < num_thread; i++) {
        fscanf(fp, "%d ", &thread_num);

        for (int j = 0; j < num_work; j++) {
            fscanf(fp, "%c %d ", &work_type, &value);

            thread_work[i][j].type = work_type;
            thread_work[i][j].value = value;
        }
    }

    fclose(fp);

    // set random seed
    srand(time(NULL));

    // initialize list
    list_init(&head, &tail);

    pthread_t threads[MAX_NUM_THREAD];

    // create threads
    for (long i = 0; i < num_thread; i++) {
        if (pthread_create(&threads[i], NULL, ThreadFunc, (void*)i) < 0) {
            exit(0);
        }
    }

    // wait threads end
    for (long i = 0; i < num_thread; i++) {
        pthread_join(threads[i], NULL);
    }

    // validate list
    FILE* fp_result = fopen("result.txt", "r");
    int size_result;
    int value_result;

    fscanf(fp_result, "%d", &size_result);

    bool is_valid = true;
    Node* it = head->next;
    for (int i = 0; i < size_result; i++) {
        if (it == tail) {
            is_valid = false;
            break;
        }
        fscanf(fp_result, "%d", &value_result);
        if (it->key != value_result) {
            is_valid = false;
            break;
        }
        it = it->next;
    }
    if (it != tail) {
        is_valid = false;
    }

    fclose(fp_result);

    if (is_valid) {
        printf("correct!\n");
    } else {
        printf("incorrect!\n");
    }

    return 0;
}

