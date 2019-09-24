#include <heap.h>
#include <stdlib.h>
#include <string.h>

void _swap(void **a, void **b) {
    void *tmp = *a;
    *a = *b;
    *b = tmp;
}

void _percolate_up(heap_t *heap) {
    off_t idx = heap->size - 1;
    byte *cur = heap->_last_ptr - heap->item_len;
    while (idx > 0) {
        byte *parent = heap->buf + ((idx + 1) / 2 - 1) * heap->item_len;
        if (heap->cmp_func(cur, parent) >= 0) {
            break;
        }
        _swap((void**)cur, (void**)parent);
        cur = parent;
        idx = ((idx + 1) / 2 - 1);
    }
}

void _percolate_down(heap_t *heap) {
    off_t idx = 0, cl;
    byte *cur = heap->buf;
    while ((cl = (idx + 1) * 2 - 1) < heap->size) {
        byte *basis = heap->buf + cl * heap->item_len, *other = heap->buf + (cl + 1) * heap->item_len;
        if ((cl + 1) < heap->size && heap->cmp_func(basis, other) > 0) {
            _swap((void**)&basis, (void**)&other);
            ++cl;
        }
        
        if (heap->cmp_func(cur, basis) <= 0) {
            break;
        }

        _swap((void**)cur, (void**)basis);
        cur = basis;
        idx = cl;
    }
}

heap_t * create_heap(size_t item_num, size_t item_len, int (*cmp_func)(void *, void *)) {
    heap_t *heap = (heap_t*)malloc(sizeof(heap_t));
    heap->bufsiz = item_num * item_len;
    heap->buf = (byte*)malloc(heap->bufsiz);
    heap->cmp_func = cmp_func;
    heap->item_num = item_num;
    heap->size = 0;
    return heap;
}

void heap_top(heap_t *heap, void *out) {
    memcpy(out, heap->buf, heap->item_len);
}

void heap_pop(heap_t *heap, void *out) {
    memcpy(out, heap->buf, heap->item_len);
    --heap->size;
    heap->_last_ptr -= heap->item_len;
    memcpy(heap->buf, heap->_last_ptr, heap->item_len);
    _percolate_down(heap);
}

void heap_push(heap_t *heap, void *value) {
    memcpy(heap->_last_ptr, value, heap->item_len);
    heap->_last_ptr += heap->item_len;
    ++heap->size;
    _percolate_up(heap);
}

void free_heap(heap_t *heap) {
    free(heap->buf);
    free(heap);
}
