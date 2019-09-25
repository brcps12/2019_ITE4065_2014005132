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

ifstream fin;
ofstream fout;

size_t file_size, total_records;

int main(int argc, char* argv[]) {
    srand(time(NULL));
    
    if (argc < 3) {
        printf("usage: %s <path to input> <path to output>\n", argv[0]);
        return 0;
    }

    TimeTracker tracker;
    // prepare input file
    fin.open(argv[1], ios::in | ios::binary);

    fin.seekg(0, ios::end);
    file_size = fin.tellg();
    fin.seekg(0, ios::beg);

    total_records = file_size / NB_RECORD;

    // prepare output file
    fout.open(argv[2], ios::trunc | ios::binary);

    byte * buffer = (byte*)malloc(file_size);

    tracker.start();
    fin.read(buffer, file_size);
    tracker.stopAndPrint("Read Time");
    
    tracker.start();
    fout.write(buffer, file_size);

    fin.close();
    fout.close();
    tracker.stopAndPrint("Write Time");
    free(buffer);

    return 0;
}
