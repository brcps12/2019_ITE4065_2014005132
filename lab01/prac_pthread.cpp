#include <stdio.h>
#include <pthread.h>

#define NUM_THREADS     10
#define NUM_INCREMENT   1000000

long cnt_global = 0;

void* thread_fhnc(void* arg) {
  long cnt_local = 0;

  for (int i = 0; i < NUM_INCREMENT; i++) {
    cnt_global++;
    cnt_local++;
  }

  return (void*)cnt_local;
}

int main(void) {
  pthread_t threads[NUM_THREADS];

  // createe threads
  for (int i = 0; i < NUM_THREADS; i++) {
    if (pthread_create(&threads[i], 0, thread_fhnc, NULL) < 0) {
      printf("error: pthread+create failed!\n");
      return 0;
    }
  }

  long ret;

  for (int i = 0; i < NUM_THREADS; i++) {
    pthread_join(threads[i], (void**)&ret);
    printf("thread %d: local count -> %ld\n", threads[i], ret);
  }

  printf("global count -> %ld\n", cnt_global);
  
  return 0;
}