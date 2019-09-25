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

#include <queue>
#include <algorithm>
#include <vector>

#include <myutil.h>
#include <time_chk.h>
#include <mytypes.h>
#include <heap.h>
#include <buffered_io.h>

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

#define RECORD_THRESHOLD 1000000
#define SORT_THRESHOLD 1000000
#define READ_THRESHOLD 10000

#define NUM_OF_THREADS (160)
// It can be set in dynamically: currently 80% of total(=2g)
#define MAX_MEMSIZ_FOR_DATA ((size_t)(0.9 * 2 * GB))
// #define MAX_MEMSIZ_FOR_DATA ((size_t)(300 * MB))
// #define MAX_MEMSIZ_FOR_DATA ((size_t)(200))
#define MAX_RECORD_NUM ((size_t)(MEMSIZ_FOR_DATA / NB_RECORD))
#define INPUT_BUFSIZ (128 * MB)
#define OUTPUT_BUFSIZ (128 * MB)

#define TMPFILE_NAME "tmp.%d"

typedef bool (*cmp_operator)(const record_t &, const record_t &);

typedef struct {
    record_t *record;
    uint k;
} heap_item_t;

int input_fd;
buffered_io_fd *fout;
// FILE *fout;

size_t record_buf_size;
size_t file_size, total_records;

byte *outbuf;
record_t *record_buf;

buffered_io_fd **tmpfiles;
// FILE **tmpfiles;

time_interval_t gbtime;

bool record_comparison(const record_t &a, const record_t &b) {
    return memcmp(&a, &b, NB_KEY) < 0;
}

// cmp_operator record_comparison_updown(bool up) {
//     return [up](const record_t &a, const record_t &b) -> bool {
//         return (memcmp(&a, &b, NB_KEY) < 0) == up;
//     };
// }

class heap_comparison {
public:
    bool operator() (const heap_item_t &a, const heap_item_t &b) {
        return memcmp(a.record, b.record, NB_KEY) > 0;
    }
};

int compare_record(const void *a, const void *b) {
    return memcmp(a, b, NB_KEY);
}

/*
int compare_record(const record_t *a, const record_t *b) {
    return memcmp(a, b, NB_KEY);
}

int compare(const void *a, const void *b) {
    return compare_record(record_buf + *(off_t*)a, record_buf + *(off_t*)b);
}

int compare_heap_item(const heap_item_t &a, const heap_item_t &b) {
    return compare_record(a.record, b.record);
}

int compare_for_sort(const off_t *a, const off_t *b) {
    return compare_record(record_buf + *a, record_buf + *b) < 0;
}

int compare_record_for_sort(const record_t &a, const record_t &b) {
    return compare_record(&a, &b) < 0;
}
*/

void swap(void **a, void **b) {
    void *tmp = *a;
    *a = *b;
    *b = tmp;
}

int rand_bet(int start, int end) {
    return (end - start + 1) * ((double)rand() / RAND_MAX) + start;
}

size_t read_records(FILE *in, void *buf, size_t len) {
    return fread(buf, NB_RECORD, len, in);
}

void partially_partition(record_t *records, off_t start, off_t end, off_t *i, off_t *j) {
    if (end - start <= 1) {
        if (memcmp(&records[start], &records[end], NB_KEY) > 0) {
            std::swap(records[start], records[end]);
        }
        *i = start,
        *j = end;
        return;
    }
    off_t it = start;
    rec_key_t pivot;
    memcpy(&pivot, &records[start].key, NB_KEY); // todo: optimize
    while (it <= end) {
        int cmp = memcmp(&records[it], &pivot, NB_KEY);
        if (cmp < 0) {
            std::swap(records[start], records[it]);
            ++start, ++it;
        } else if (cmp == 0) {
            ++it;
        } else {
            std::swap(records[it], records[end]);
            --end;
        }
    }
    *i = start - 1;
    *j = it;
}

void partially_quicksort(record_t *records, off_t start, off_t end, rec_key_t *pv = NULL) {
    if (end - start <= SORT_THRESHOLD) {
        std::sort(records + start, records + end + 1, record_comparison);
    } else {
        if (start >= end) return;
        off_t i, j; 
        
        rec_key_t pivot;
        memcpy(&pivot, &records[rand_bet(start, end)].key, NB_KEY);
        record_t *ptr = std::partition(records + start, records + end + 1, [pivot](const record_t &a) -> bool { 
            return memcmp(&a, &pivot, NB_KEY) < 0;
        });
        // partially_partition(records, start, end, &i, &j);

        #pragma omp task
        {
            // partially_quicksort(records, start, i);
            partially_quicksort(records, start, (ptr - records) - 1);
        }

        // #pragma omp task
        // {
            // partially_quicksort(records, j, end);
            partially_quicksort(records, (ptr - records), end);
        // }
    }
}

void read_and_sort(off_t start, off_t offset, size_t maxlen) {
    // time_interval_t tin;
    // begin_time_track(&tin);
    // len = read_records(fin, record_buf + start, len);
    size_t readbytes = pread(input_fd, record_buf + start, maxlen * NB_RECORD, (offset + start) * NB_RECORD);
    size_t len = readbytes / NB_RECORD;
    // stop_and_print_interval(&tin, "Read");

    // begin_time_track(&tin);
    // std::sort(record_offs + start, record_offs + start + len, compare_for_sort);
    std::sort(record_buf + start, record_buf + start + len, record_comparison);
    // qsort(record_buf + start, len, sizeof(record_t), compare_record);
    // qsort(record_offs + start, len, sizeof(off_t), compare);
    // partially_quicksort(record_buf, start, start + len - 1);
    // stop_and_print_interval(&tin, "Each Part Sort");
}

// void twoway_merge(off_t *offin, off_t *offout, off_t start, off_t mid, off_t end) {
//     off_t l = start, r = mid + 1, i = start;

//     while (l <= mid && r <= end) {
//         int res = compare(offin + l, offin + r);
//         if (res < 0) {
//             offout[i++] = offin[l++];
//         } else {
//             offout[i++] = offin[r++];
//         }
//     }

//     while (l <= mid) {
//         offout[i++] = offin[l++];
//     }

//     while (r <= end) {
//         offout[i++] = offin[r++];
//     }
// }

void kway_merge(buffered_io_fd *out, record_t *rin, size_t buflen, off_t k, off_t len) {
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
        buffered_append(out, p.record, sizeof(record_t));
        ++ptrs[p.k];
        if (ptrs[p.k] != mxidx[p.k]) {
            q.push({ ptrs[p.k], p.k });
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

void bitonic_compare(record_t *records, size_t len, bool up) {
    size_t dist = len / 2;
    for (int i = 0; i < dist; i++) {
        if (record_comparison(records[i + dist], records[i]) == up) {
            std::swap(records[i], records[i + dist]);
        }
    }
}

void bitonic_merge(record_t *records, size_t len, bool up) {
        if (len == 1) return;
    if (len <= SORT_THRESHOLD) {
        bitonic_compare(records, len, up);
        bitonic_merge(records, len / 2, up);
        bitonic_merge(records + len / 2, len - len / 2, up);
    } else {
        bitonic_compare(records, len, up);

        #pragma omp task
        {
            bitonic_merge(records, len / 2, up);
        }

        #pragma omp task
        {
            bitonic_merge(records + len / 2, len - len / 2, up);
        }
    }
}

void bitonic_sort(record_t *records, size_t len, bool up) {
    if (len <= SORT_THRESHOLD) {
        std::sort(records, records + len, [up](const record_t &a, const record_t &b) -> bool {
            return (memcmp(&a, &b, NB_KEY) < 0) == up;
        });
    } else {
        #pragma omp task
        {
            bitonic_sort(records, len / 2, true);
        }

        #pragma omp task
        {
            bitonic_sort(records + len / 2, len - (len / 2), false);
        }

        bitonic_merge(records, len, up);
    }

    // int count[256] = { 0, };
    // int idxes[256] = { 0, };
    // record_t *ptr = records + start;
    // while(ptr < records + end) {
    //     ++count[*((unsigned char*)(ptr++) + radix)];
    // }

    // for (int i = 1; i < 256; i++) {
    //     count[i] += count[i - 1];
    //     idxes[i] = count[i];
    // }

    // int num = 0;
    // record_t tmp = records[start];
    // while (num < end - start) {
    //     std::swap(records[--count[*((unsigned char*)&tmp + radix)]], tmp);
    //     ++num;
    // }

    // for (int i = 0; i < 256; i++) {
    //     off_t next = i == 255 ? end : idxes[i + 1];
    //     if (idxes[i] < next) {
    //         partially_radixsort(records, start + idxes[i], start + next, radix + 1);
    //     }
    // }
}

void parallel_bitonic_sort(record_t *records, size_t len) {
    #pragma omp parallel
    {
        #pragma omp single nowait
        {
            bitonic_sort(records, len, true);
        }
    }
}

void partial_sort(buffered_io_fd *out, off_t offset, size_t num_records) {
    time_interval_t tin;
    begin_time_track(&tin);
    // #pragma omp parallel for
    // for (off_t start = 0; start < num_records; start += RECORD_THRESHOLD) {
    //     size_t maxlen = start + RECORD_THRESHOLD >= num_records ? num_records - start : RECORD_THRESHOLD;
    //     pread(input_fd, record_buf + start, maxlen * NB_RECORD, (offset + start) * NB_RECORD);
    //     parallel_quicksort(record_buf + start, 0, maxlen - 1);
    // }
    // for (off_t start = 0; start < num_records; start += RECORD_THRESHOLD) {
    //     size_t maxlen = start + RECORD_THRESHOLD >= num_records ? num_records - start : RECORD_THRESHOLD;
    //     read_and_sort(start, offset, maxlen);
    // }
    // #pragma omp parallel for
    // for (off_t start = 0; start < num_records; start += READ_THRESHOLD) {
    //     size_t maxlen = start + READ_THRESHOLD >= num_records ? num_records - start : READ_THRESHOLD;
    //     pread(input_fd, record_buf + start, maxlen * NB_RECORD, (offset + start) * NB_RECORD);
    // }
    // pread(input_fd, record_buf, num_records * NB_RECORD, offset * NB_RECORD);
    read(input_fd, record_buf, num_records * NB_RECORD);
    // record_t *buf = (record_t*)mmap(0, num_records * NB_RECORD, PROT_READ | PROT_WRITE, MAP_PRIVATE, input_fd, 0);
    // memcpy(record_buf, buf, num_records * NB_RECORD);
    stop_and_print_interval(&tin, "All Read");
    begin_time_track(&tin);
    parallel_quicksort(record_buf, 0, num_records - 1);
    // partially_radixsort(record_buf, 0, num_records, 0);
    // bitonic_sort(record_buf, num_records, true);
    stop_and_print_interval(&tin, "All Partially Sorted");
    
    // begin_time_track(&tin);
    // int k = num_records / RECORD_THRESHOLD + (num_records % RECORD_THRESHOLD != 0);
    // for (size_t mlen = RECORD_THRESHOLD; mlen < num_records; mlen <<= 1) {
    //     #pragma omp parallel for
    //     for (off_t start = 0; start < num_records; start += (mlen << 1)) {
    //         off_t end = start + (mlen << 1) - 1;
    //         off_t mid = start + mlen - 1;
    //         if (mid >= num_records) {
    //             mid = end = num_records - 1;
    //         } else if (end >= num_records) {
    //             end = num_records - 1;
    //         }

    //         twoway_merge(offin, offout, start, mid, end);
    //     }

    //     swap((void**)&offin, (void**)&offout);
    // }
    // kway_merge(out, record_buf, num_records, k, RECORD_THRESHOLD);
    // stop_and_print_interval(&tin, "Merge");
    
    begin_time_track(&tin);
    // for (off_t i = 0; i < num_records; i ++) {
    //     off_t idx = offin[i];
    //     fwrite(record_buf + idx, NB_RECORD, 1, out);
    // }

    write(out->fd, record_buf, num_records * NB_RECORD);
    buffered_flush(out);
    // munmap(buf, num_records * NB_RECORD);
    // #pragma omp parallel for
    // for (off_t start = 0; start < num_records; start += RECORD_THRESHOLD) {
    //     size_t maxlen = start + RECORD_THRESHOLD >= num_records ? num_records - start : RECORD_THRESHOLD;
    //     pwrite(out->fd, record_buf + start, maxlen * NB_RECORD, (offset + start) * NB_RECORD);
    // }
    stop_and_print_interval(&tin, "File write");
}

record_t *get_next_record(buffered_io_fd *in, record_t *buf, record_t **ptr, size_t bufsiz, ssize_t *remain) {
    if (*remain == 0) {
        *remain = buffered_read(in, buf, bufsiz * NB_RECORD) / NB_RECORD;
        if (*remain <= 0) return NULL;
        *ptr = buf;
    }

    --*remain;
    *ptr += 1;
    return *ptr - 1;
}

// void kway_external_merge(buffered_io_fd **tmpfiles, buffered_io_fd *out, size_t k) {
//     time_interval_t tin;
//     begin_time_track(&tin);
//     record_t *bufs[k], *ptrs[k], *record;
//     ssize_t remains[k] = { 0, };
//     size_t bufsiz = record_buf_size / (NB_RECORD * k);
//     std::priority_queue<heap_item_t, std::vector<heap_item_t>, heap_comparison> q;
//     for (int i = 0; i < k; i++) {
//         bufs[i] = record_buf + i * bufsiz;
//         record = get_next_record(tmpfiles[i], bufs[i], &ptrs[i], bufsiz, &remains[i]);
//         q.push({ record, i });
//     }

//     while (!q.empty()) {
//         heap_item_t p = q.top();
//         q.pop();
//         buffered_append(out, p.record, sizeof(record_t));
//         record = get_next_record(tmpfiles[p.k], bufs[p.k], &ptrs[p.k], bufsiz, &remains[p.k]);
//         if (record != NULL) {
//             q.push({ record, p.k });
//         }
//     }
//     buffered_flush(out);
//     stop_and_print_interval(&tin, "External Merge");
// }

void kway_external_merge(buffered_io_fd **tmpfiles, buffered_io_fd *out, size_t k) {
    time_interval_t tin;
    begin_time_track(&tin);
    record_t *bufs[k], *ptrs[k], *record;
    ssize_t remains[k] = { 0, };
    size_t bufsiz = record_buf_size / (NB_RECORD * k);
    std::priority_queue<heap_item_t, std::vector<heap_item_t>, heap_comparison> q;
    for (int i = 0; i < k; i++) {
        bufs[i] = record_buf + i * bufsiz;
        record = get_next_record(tmpfiles[i], bufs[i], &ptrs[i], bufsiz, &remains[i]);
        q.push({ record, i });
    }

    while (!q.empty()) {
        heap_item_t p = q.top();
        q.pop();
        buffered_append(out, p.record, sizeof(record_t));
        record = get_next_record(tmpfiles[p.k], bufs[p.k], &ptrs[p.k], bufsiz, &remains[p.k]);
        if (record != NULL) {
            q.push({ record, p.k });
        }
    }
    buffered_flush(out);
    stop_and_print_interval(&tin, "External Merge");
}

// void external_merge(buffered_io_fd *fin_left, buffered_io_fd *fin_right, buffered_io_fd *fout) {
//     // simple
//     time_interval_t tin;
//     begin_time_track(&tin);

//     size_t bufsiz = record_buf_size / (NB_RECORD * 2);
//     size_t lremain = 0, rremain = 0;
//     record_t *lbuf = record_buf, *rbuf = record_buf + bufsiz;
//     record_t *lp = NULL, *rp = NULL;
//     record_t *l = get_next_record(fin_left, lbuf, &lp, bufsiz, &lremain);
//     record_t *r = get_next_record(fin_right, rbuf, &rp, bufsiz, &rremain);
    
//     // record_t prev, cur;
//     // size_t it = 0;
//     while (l && r) {
//         // prev = cur;
//         if (record_comparison(*l, *r)) {
//             // fwrite(l, NB_RECORD, 1, fout);
//             buffered_append(fout, l, NB_RECORD);
//             // cur = *l;
//             l = get_next_record(fin_left, lbuf, &lp, bufsiz, &lremain);
//         } else {
//             // fwrite(r, NB_RECORD, 1, fout);
//             buffered_append(fout, r, NB_RECORD);
//             // cur = *r;
//             r = get_next_record(fin_right, rbuf, &rp, bufsiz, &rremain);
//         }
//         // ++it;

//         // if (it > 1) {
//             // assert(compare_record(&prev, &cur) <= 0);
//         // }
//     }

//     while (l) {
//         // prev = cur;
//         buffered_append(fout, l, NB_RECORD);
//         // cur = *l;
//         l = get_next_record(fin_left, lbuf, &lp, bufsiz, &lremain);
//         // ++it;

//         // if (it > 1) {
//             // assert(compare_record(&prev, &cur) <= 0);
//         // }
//     }

//     while (r) {
//         // prev = cur;
//         buffered_append(fout, r, NB_RECORD);
//         // cur = *r;
//         r = get_next_record(fin_right, rbuf, &rp, bufsiz, &rremain);
//         // ++it;

//         // if (it > 1) {
//             // assert(compare_record(&prev, &cur) <= 0);
//         // }
//     }
    
//     // fflush(fout);
//     buffered_flush(fout);
//     stop_and_print_interval(&tin, "External Merge");
// }

int main(int argc, char* argv[]) {
    srand(time(NULL));
    if (argc < 3) {
        printf("usage: %s <path to input> <path to output>\n", argv[0]);
        return 0;
    }

#ifdef LOCAL_TEST
    printf("This runs in local test only\n");
    // char *num_thread = getenv("MP_NUM_OF_THREAD");
    // omp_set_num_threads(atoi(num_thread));
    omp_set_num_threads(NUM_OF_THREADS);
#else
    omp_set_num_threads(NUM_OF_THREADS);
#endif

    outbuf = (byte*)malloc(OUTPUT_BUFSIZ);
    input_fd = open(argv[1], O_RDONLY);
    if (input_fd == -1) {
        printf("error: cannot open file\n");
        return -1;
    }

    file_size = lseek(input_fd, 0, SEEK_END);
    total_records = file_size / NB_RECORD;

    fout = buffered_open(argv[2], O_RDWR | O_CREAT | O_TRUNC, outbuf, OUTPUT_BUFSIZ);
    // fout = fopen(argv[2], "wb+");
    if (fout == NULL) {
        printf("error: cannot create output file\n");
        return -1;
    }

    pwrite(fout->fd, "\0", 1, file_size - 1);

    record_buf_size = min(total_records * NB_RECORD, MAX_MEMSIZ_FOR_DATA);
    record_buf = (record_t*)malloc(record_buf_size);

    size_t num_record_for_partition = record_buf_size / NB_RECORD;
    size_t num_partition = total_records / num_record_for_partition + (total_records % num_record_for_partition != 0);

    lseek(input_fd, 0, SEEK_SET);
    
    if (num_partition <= 1) {
        // setvbuf(fout, outbuf, _IOFBF, OUTPUT_BUFSIZ);
        partial_sort(fout, 0, total_records);
    } else {
        tmpfiles = (buffered_io_fd **)malloc(num_partition * sizeof(buffered_io_fd*));
        char name[15];
        for (int i = 0; i < num_partition; i++) {
            sprintf(name, TMPFILE_NAME, i);
            tmpfiles[i] = buffered_open(name, O_RDWR | O_CREAT | O_TRUNC, outbuf, OUTPUT_BUFSIZ);
            // tmpfiles[i] = fopen(name, "wb+");
        }

        int tidx = 0;
        for (off_t offset = 0; offset < total_records; offset += num_record_for_partition, ++tidx) {
            size_t num_records = min(offset + num_record_for_partition, total_records) - offset;
            partial_sort(tmpfiles[tidx], offset, num_records);
        }

        for (int i = 0; i < num_partition; i++) {
            lseek(tmpfiles[i]->fd, 0, SEEK_SET);
            buffered_reset(tmpfiles[i]);
        }
        
        kway_external_merge(tmpfiles, fout, num_partition);
    }

    close(input_fd);
    time_interval_t tin;

    begin_time_track(&tin);
    if (num_partition > 1) {
        char name[15];
        for (off_t i = 0; i < num_partition; i++) {
            sprintf(name, TMPFILE_NAME, i);
            buffered_close(tmpfiles[i]);
            remove(name);
        }
        free(tmpfiles);
    }
    stop_and_print_interval(&tin, "Flush File");

    begin_time_track(&tin);
    buffered_close(fout);
    stop_and_print_interval(&tin, "Flush File");

    free(outbuf);
    free(record_buf);

    return 0;
}
