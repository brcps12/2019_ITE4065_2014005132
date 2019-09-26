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
#include <bufio.hpp>

using namespace std;

#define THRESHOLD_SORT 1000000
#define MAX_THREAD_NUM 8
#define BYTE_SIZE 256

// It can be set in dynamically: currently 80% of total(=2g)
#define MAX_MEMSIZ_FOR_DATA ((size_t)(0.9 * 2 * GB))
// #define MAX_MEMSIZ_FOR_DATA ((size_t)(300 * MB))
// #define MAX_MEMSIZ_FOR_DATA ((size_t)(200))

#define INPUT_BUFSIZ (128 * MB)
#define OUTPUT_BUFSIZ (128 * MB)

#define TMPFILE_NAME "tmp.%d"

typedef struct {
    record_t *record;
    int k;
} heap_item_t;

BufferedIO input, output;

size_t file_size, total_records;

size_t recbuf_size;
record_t *recbuf;

size_t outbuf_size;
record_t *outbuf;

record_t *inmap, *outmap;

BufferedIO *tmpfiles;

class heap_comparison {
public:
    bool operator() (const heap_item_t &a, const heap_item_t &b) {
        return memcmp(a.record, b.record, NB_KEY) > 0;
    }
};

inline bool record_comparison(const record_t &a, const record_t &b) {
    return memcmp(&a, &b, NB_KEY) < 0;
}

inline void read_records(record_t *buf, size_t len, off_t offset) {
    // memcpy(buf, inmap + offset, len * NB_RECORD);
    pread(input.getfd(), buf, len * NB_RECORD, offset * NB_RECORD);
}

inline void radix_sort(record_t *buf, int len, int which) {
    // use 1 byte
    record_t *last_[BYTE_SIZE + 1];
    record_t **last = last_ + 1;
    int count[BYTE_SIZE] = { 0, };

    for (record_t *ptr = buf; ptr < buf + len; ++ptr) {
        ++count[(unsigned char)ptr->key[which]];
    }

    last_[0] = last_[1] = buf;
    for (int i = 1; i < BYTE_SIZE; ++i) {
        last[i] = last[i-1] + count[i-1];
    }

    record_t *e = buf + len;
    for (int i = 0; i < BYTE_SIZE; ++i) {
        record_t *end = last[i-1] + count[i];
        if (end == e) { 
            last[i] = buf + len;
            break;
        }

        while (last[i] != end) {
            record_t swapper = *last[i];
            unsigned char tag = (unsigned char)swapper.key[which];
            if (tag != i) {
                do {
                    swap(swapper, *last[tag]++);
                } while ((tag = (unsigned char)swapper.key[which]) != i);
                *last[i] = swapper;
            }
            ++last[i];
        }
    }

    if (which < NB_KEY - 1) {
        for (int i = 0; i < BYTE_SIZE; ++i) {
            if (count[i] > 1) {
                radix_sort(last[i - 1], last[i] - last[i - 1], which + 1);
            }
        }
    }
}

void kway_merge(BufferedIO &out, record_t *rin, size_t buflen, off_t k, off_t len) {
    record_t *mxidx[k], *ptrs[k];
    std::priority_queue<heap_item_t, std::vector<heap_item_t>, heap_comparison> q;
    for (int i = 0; i < k; i++) {
        mxidx[i] = min(rin + buflen, rin + (i + 1) * len);
        ptrs[i] = rin + i * len;
        q.push({ ptrs[i], i });
    }

    while (!q.empty()) {
        heap_item_t p = q.top();
        q.pop();
        out.append(p.record, sizeof(record_t));
        ++ptrs[p.k];
        if (ptrs[p.k] != mxidx[p.k]) {
            q.push({ ptrs[p.k], p.k });
        }
    }
}

inline void sort_phase(BufferedIO &out, int recnum, off_t offset) {
    TimeTracker tracker;
    tracker.start();
    #pragma omp parallel for
    for (off_t start = 0; start < recnum; start += THRESHOLD_SORT) {
        size_t maxlen = start + THRESHOLD_SORT >= recnum ? recnum - start : THRESHOLD_SORT;
        read_records(recbuf + start, maxlen, offset + start);
        radix_sort(recbuf + start, maxlen, 0);
    }
    tracker.stopAndPrint("Read And Sort");

    tracker.start();
    int k = recnum / THRESHOLD_SORT + (recnum % THRESHOLD_SORT != 0);
    kway_merge(out, recbuf, recnum, k, THRESHOLD_SORT);

    out.flush();
    tracker.stopAndPrint("Merge And Write");
}

void partially_quicksort(record_t *records, int start, off_t end) {
    if (end - start <= THRESHOLD_SORT) {
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

record_t *get_next_record(BufferedIO &in, record_t *buf, record_t **ptr, size_t bufsiz, ssize_t *remain) {
    if (*remain == 0) {
        *remain = in.read(buf, bufsiz * NB_RECORD) / NB_RECORD;
        if (*remain <= 0) return NULL;
        *ptr = buf;
    }

    --*remain;
    *ptr += 1;
    return *ptr - 1;
}

void kway_external_merge(BufferedIO tmpfiles[], BufferedIO &out, int k) {
    TimeTracker tracker;
    tracker.start();
    record_t *bufs[k], *ptrs[k], *record;
    ssize_t remains[k] = { 0, };
    size_t bufsiz = recbuf_size / (NB_RECORD * k);
    std::priority_queue<heap_item_t, std::vector<heap_item_t>, heap_comparison> q;
    for (int i = 0; i < k; i++) {
        bufs[i] = recbuf + i * bufsiz;
        record = get_next_record(tmpfiles[i], bufs[i], &ptrs[i], bufsiz, &remains[i]);
        q.push({ record, i });
    }

    while (!q.empty()) {
        heap_item_t p = q.top();
        q.pop();
        out.append(p.record, sizeof(record_t));
        record = get_next_record(tmpfiles[p.k], bufs[p.k], &ptrs[p.k], bufsiz, &remains[p.k]);
        if (record != NULL) {
            q.push({ record, p.k });
        }
    }

    out.flush();
    tracker.stopAndPrint("External Merge");
}

int main(int argc, char* argv[]) {
    srand(time(NULL));
    
    if (argc < 3) {
        printf("usage: %s <path to input> <path to output>\n", argv[0]);
        return 0;
    }

    TimeTracker tracker;
    // prepare input file
    input.openfile(argv[1], O_RDONLY);

    file_size = input.filesize();
    total_records = file_size / NB_RECORD;

    // prepare output file
    output.openfile(argv[2], O_CREAT | O_RDWR | O_TRUNC);
    pwrite(output.getfd(), "", 1, file_size - 1);

    inmap = (record_t *)mmap(NULL, file_size, PROT_READ, MAP_SHARED, input.getfd(), 0);
    outmap = (record_t *)mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, output.getfd(), 0);

    recbuf_size = min(MAX_MEMSIZ_FOR_DATA, file_size);
    recbuf = (record_t *)malloc(recbuf_size);

    outbuf_size = file_size > MAX_MEMSIZ_FOR_DATA ? OUTPUT_BUFSIZ : max(OUTPUT_BUFSIZ, MAX_MEMSIZ_FOR_DATA - file_size);
    outbuf = (record_t *)malloc(outbuf_size);

    output.setbuf((byte *)outbuf, outbuf_size);

    int num_record_in_partiiton = recbuf_size / NB_RECORD;
    int num_partition = total_records / num_record_in_partiiton + (total_records % num_record_in_partiiton != 0);

    if (num_partition == 1) {
        // it can be sorted in-memory
        sort_phase(output, total_records, 0);
    } else {
        tmpfiles = new BufferedIO[num_partition];
        char name[15];
        for (int i = 0; i < num_partition; i++) {
            sprintf(name, TMPFILE_NAME, i);
            tmpfiles[i].openfile(name, O_RDWR | O_CREAT | O_TRUNC);
            tmpfiles[i].setbuf((byte *)outbuf, outbuf_size);
        }

        int tidx = 0;
        for (off_t offset = 0; offset < total_records; offset += num_record_in_partiiton, ++tidx) {
            size_t recnum = min((size_t)(offset + num_record_in_partiiton), (size_t)total_records) - offset;
            sort_phase(tmpfiles[tidx], recnum, offset);
        }

        for (int i = 0; i < num_partition; i++) {
            tmpfiles[i].reset();
        }
        
        kway_external_merge(tmpfiles, output, num_partition);
    }
    
    tracker.start();
    // msync(out, file_size, MS_SYNC);

    // release
    input.closefile();
    output.closefile();
    munmap(inmap, file_size);
    munmap(outmap, file_size);

    if (num_partition > 1) {
        char name[15];
        for (off_t i = 0; i < num_partition; i++) {
            sprintf(name, TMPFILE_NAME, i);
            tmpfiles[i].closefile();
        }

        delete tmpfiles;
    }

    free(outbuf);
    free(recbuf);
    tracker.stopAndPrint("Write Time");

    return 0;
}
