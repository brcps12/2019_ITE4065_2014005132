#ifndef __HEAP_H
#define __HEAP_H

#include <sys/types.h>
#include <queue>

typedef struct {
    size_t item_num, item_len, length;
    char *buf;
    int (*cmp_func)(void *, void *);
} heap_t;

heap_t * create_heap(size_t item_num, size_t item_len, int (*cmp_func)(void *, void *));

void * heap_top(heap_t *heap);

void * heap_push(heap_t *heap, void *value);

#endif