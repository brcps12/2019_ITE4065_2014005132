#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/time.h>

#include <iostream>
#include <fstream>
#include <queue>
#include <algorithm>
#include <iterator>
#include <vector>

#include <mytypes.hpp>
#include <myutil.hpp>
#include <time_chk.hpp>

using namespace std;

#define THRESHOLD_SORT 10000
#define SORT_THRESHOLD 1000000
#define MAX_THREAD_NUM 8

// It can be set in dynamically: currently 80% of total(=2g)
#define MAX_MEMSIZ_FOR_DATA ((size_t)(0.9 * 2 * GB))
// #define MAX_MEMSIZ_FOR_DATA ((size_t)(300 * MB))
// #define MAX_MEMSIZ_FOR_DATA ((size_t)(200))

#define INPUT_BUFSIZ (128 * MB)
#define OUTPUT_BUFSIZ (128 * MB)

#define TMPFILE_NAME "tmp.%d"

typedef struct {
    record_t record;
    uint k;
} heap_item_t;

int input_fd, output_fd;

size_t file_size, total_records;

bool record_comparison(const record_t &a, const record_t &b) {
    return memcmp(&a, &b, NB_KEY) < 0;
}


void partially_quicksort(record_t *records, off_t start, off_t end) {
    if (end - start <= SORT_THRESHOLD) {
        std::sort(records + start, records + end + 1, record_comparison);
    } else {
        if (start >= end) return;
        off_t i, j; 
            
        rec_key_t pivot;
        memcpy(&pivot, &records[start + (end - start + 1) / 2].key, NB_KEY);
        record_t *ptr = std::partition(records + start, records + end + 1, [pivot](const record_t &a) -> bool { 
            return memcmp(&a, &pivot, NB_KEY) < 0;
        });
        // partially_partition(records, start, end, &i, &j);

        #pragma omp task
        {
            // partially_quicksort(records, start, i);
            partially_quicksort(records, start, (ptr - records) - 1);
        }

        #pragma omp task
        {
            // partially_quicksort(records, j, end);
            partially_quicksort(records, (ptr - records), end);
        }
    }
}

void parallel_quicksort(record_t *records, off_t start, off_t end) {
    #pragma omp parallel
    {
        #pragma omp single nowait
        {
            partially_quicksort(records, start, end);
        }
    }
}

int main(int argc, char* argv[]) {
    srand(time(NULL));
    
    if (argc < 3) {
        printf("usage: %s <path to input> <path to output>\n", argv[0]);
        return 0;
    }

    TimeTracker tracker;
    // prepare input file
    input_fd = open(argv[1], O_RDONLY);

    file_size = lseek(input_fd, 0, SEEK_END);
    total_records = file_size / NB_RECORD;

    // prepare output file
    output_fd = open(argv[2], O_CREAT | O_RDWR | O_TRUNC, 0777);
    pwrite(output_fd, "", 1, file_size - 1);

    byte *in = (byte *)mmap(NULL, file_size, PROT_READ, MAP_SHARED, input_fd, 0);
    byte *out = (byte *)mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, output_fd, 0);
    
    tracker.start();
    memcpy(out, in, file_size);
    munmap(in, file_size);
    tracker.stopAndPrint("Copy Time");

    tracker.start();
    parallel_quicksort((record_t*)out, 0, total_records - 1);
    tracker.stopAndPrint("Read And Sort");
    
    tracker.start();
    msync(out, file_size, MS_SYNC);

    close(input_fd);
    close(output_fd);
    munmap(out, file_size);
    tracker.stopAndPrint("Write Time");

    return 0;
}
