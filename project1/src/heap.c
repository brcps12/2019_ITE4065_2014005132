#include <heap.h>
#include <stdlib.h>

heap_t * create_heap(size_t item_num, size_t item_len, int (*cmp_func)(void *, void *)) {
    heap_t *heap = (heap_t*)malloc(sizeof(heap_t));
    heap->cmp_func = cmp_func;
    heap->item_num = item_num;
}

void * heap_top(heap_t *heap) {
}

void * heap_pop(heap_t *heap) {

}

void * heap_push(heap_t *heap, void *value) {

}

void _percolate_up() {

}

void _percolate_down() {

}
