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

    tracker.start();
    byte *in = (byte *)mmap(NULL, file_size, PROT_READ, MAP_SHARED, input_fd, 0);
    tracker.stopAndPrint("Read Time");
    
    tracker.start();
    byte *out = (byte *)mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, output_fd, 0);
    memcpy(out, in, file_size);
    msync(out, file_size, MS_SYNC);

    close(input_fd);
    close(output_fd);
    munmap(in, file_size);
    munmap(out, file_size);
    tracker.stopAndPrint("Write Time");

    return 0;
}
