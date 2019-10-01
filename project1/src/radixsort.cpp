#include <algorithm>
#include <string.h>
#include <radixsort.hpp>

#define BYTE_SIZE 256
#define THRESHOLD_SORT 500
#define THRESHOLD_TASK 10000

void radixsort(record_t *records, int len, int byteidx) {
    if (len <= THRESHOLD_SORT) {
        std::sort(records, records + len, [byteidx](record_t &a, record_t &b) {
            return memcmp(&a.key[byteidx], &b.key[byteidx], NB_KEY - byteidx) < 0;
        });
        return;
    }

    record_t *last_[BYTE_SIZE + 1];
    record_t **last = last_ + 1;
    int count[BYTE_SIZE] = { 0, };

    for (record_t *ptr = records; ptr < records + len; ++ptr) {
        ++count[ptr->key[byteidx]];
    }

    last_[0] = last_[1] = records;
    for (int i = 1; i < BYTE_SIZE; ++i) {
        last[i] = last[i-1] + count[i-1];
    }

    record_t *e = records + len;
    for (int i = 0; i < BYTE_SIZE; ++i) {
        record_t *end = last[i-1] + count[i];
        if (end == e) { 
            last[i] = records + len;
            break;
        }

        while (last[i] != end) {
            record_t swapper = *last[i];
            byte tag = swapper.key[byteidx];
            if (tag != i) {
                do {
                    std::swap(swapper, *last[tag]++);
                } while ((tag = swapper.key[byteidx]) != i);
                *last[i] = swapper;
            }
            ++last[i];
        }
    }

    if (byteidx < NB_KEY - 1) {
        if (byteidx == 0) {
            #pragma omp parallel for shared(count, last, byteidx)
            for (int i = 0; i < BYTE_SIZE; ++i) {
                if (count[i] > 1) {
                    radixsort(last[i - 1], last[i] - last[i - 1], byteidx + 1);
                }
            }
        } else if (len > THRESHOLD_TASK) {
            for (int i = 0; i < BYTE_SIZE; ++i) {
                if (count[i] > 1) {
                    #pragma omp task
                    radixsort(last[i - 1], last[i] - last[i - 1], byteidx + 1);
                }
            }
            #pragma omp taskwait
        } else {
            for (int i = 0; i < BYTE_SIZE; ++i) {
                if (count[i] > 1) {
                    radixsort(last[i - 1], last[i] - last[i - 1], byteidx + 1);
                }
            }
        }
    }
}
