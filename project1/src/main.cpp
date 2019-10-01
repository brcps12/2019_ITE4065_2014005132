#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
// #include <sys/time.h>
#include <queue>
#include <algorithm>
#include <vector>
#include <aio.h>
#include <errno.h>

// #include <time_chk.hpp>
#include <mytypes.hpp>
#include <bufio.hpp>
#include <myutil.hpp>

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

#define RECORD_THRESHOLD 1000000
#define SORT_THRESHOLD 500
#define BYTE_SIZE 256

#define NUM_OF_THREADS (80)
// It can be set in dynamically: currently 80% of total(=2g)
#define MAX_MEMSIZ_FOR_DATA ((size_t)(0.9 * 2 * GB))
// #define MAX_MEMSIZ_FOR_DATA ((size_t)(300 * MB))
// #define MAX_MEMSIZ_FOR_DATA ((size_t)(200))
#define MAX_RECORD_NUM ((size_t)(MEMSIZ_FOR_DATA / NB_RECORD))
#define INPUT_BUFSIZ (64 * MB)
#define OUTPUT_BUFSIZ (64 * MB)

#define TMPFILE_NAME "tmp.%d"

typedef struct {
    record_t *record;
    off_t k;
} heap_item_t;

typedef struct {
    buffered_io_fd *file;
    record_t *buf[2], *ptr;
    size_t bufsiz[2], num;
    ssize_t remain[2];
    int cur;
    struct aiocb aio;
} external_t;

int input_fd;
buffered_io_fd *fout;
// FILE *fout;

size_t record_buf_size;
size_t file_size, total_records;

size_t outbuf_size;
byte *outbuf;
record_t *record_buf;

buffered_io_fd **tmpfiles;
// FILE **tmpfiles;

bool record_comparison(record_t &a, record_t &b) {
    return memcmp(&a, &b, NB_KEY) < 0;
}

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

size_t read_records(FILE *in, void *buf, size_t len) {
    return fread(buf, NB_RECORD, len, in);
}

// void partially_partition(record_t *records, off_t start, off_t end, off_t *i, off_t *j) {
//     if (end - start <= 1) {
//         if (compare_record(&records[start], &records[end]) > 0) {
//             std::swap(records[start], records[end]);
//         }
//         *i = start,
//         *j = end;
//         return;
//     }
//     off_t it = start;
//     record_t pivot = records[start]; // todo: optimize
//     while (it <= end) {
//         int cmp = compare_record(&records[it], &pivot);
//         if (cmp < 0) {
//             std::swap(records[start], records[it]);
//             ++start, ++it;
//         } else if (cmp == 0) {
//             ++it;
//         } else {
//             std::swap(records[it], records[end]);
//             --end;
//         }
//     }
//     *i = start - 1;
//     *j = it;
// }

// void partially_quicksort(record_t *records, off_t start, off_t end) {
//     if (start >= end) return;
//     off_t i, j; 
    
//     partially_partition(records, start, end, &i, &j);
//     partially_quicksort(records, start, i);
//     partially_quicksort(records, j, end);
// }

void radix_sort(record_t *buf, int len, int which) {
    if (len <= SORT_THRESHOLD) {
        std::sort(buf, buf + len, [which](record_t &a, record_t &b) {
            return memcmp(&a.key[which], &b.key[which], NB_KEY - which) < 0;
        });
        return;
    }
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
                    std::swap(swapper, *last[tag]++);
                } while ((tag = (unsigned char)swapper.key[which]) != i);
                *last[i] = swapper;
            }
            ++last[i];
        }
    }

    if (which < NB_KEY - 1) {
        if (which == 0) {
            #pragma omp parallel for shared(count, last, which)
            for (int i = 0; i < BYTE_SIZE; ++i) {
                if (count[i] > 1) {
                    radix_sort(last[i - 1], last[i] - last[i - 1], which + 1);
                }
            }
        } else if (len > 10000) {
            for (int i = 0; i < BYTE_SIZE; ++i) {
                if (count[i] > 1) {
                    #pragma omp task
                    radix_sort(last[i - 1], last[i] - last[i - 1], which + 1);
                }
            }
            #pragma omp taskwait
        } else {
            for (int i = 0; i < BYTE_SIZE; ++i) {
                if (count[i] > 1) {
                    radix_sort(last[i - 1], last[i] - last[i - 1], which + 1);
                }
            }
        }
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
    radix_sort(record_buf + start, len, 0);
    // std::sort(record_buf + start, record_buf + start + len, record_comparison);
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

bool isSorted(record_t *start, record_t *end) {
    for (record_t *ptr = start + 1; ptr != end; ptr++) {
        if (memcmp(ptr - 1, ptr, NB_KEY) > 0) {
            return false;
        }
    }
    return true;
}

void partial_sort(buffered_io_fd *out, off_t offset, size_t num_records, size_t write_offset) {
    // time_interval_t tin;
    // begin_time_track(&tin);
    #pragma omp parallel for
    for (off_t start = 0; start < num_records; start += RECORD_THRESHOLD) {
        size_t maxlen = start + RECORD_THRESHOLD >= num_records ? num_records - start : RECORD_THRESHOLD;
        pread(input_fd, record_buf + start, maxlen * NB_RECORD, (offset + start) * NB_RECORD);
        // read_and_sort(start, offset, maxlen);
    }
    // stop_and_print_interval(&tin, "All Partially Sorted");
    
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

    radix_sort(record_buf, num_records, 0);
    // stop_and_print_interval(&tin, "Merge");
    
    // begin_time_track(&tin);
    // for (off_t i = 0; i < num_records; i ++) {
    //     off_t idx = offin[i];
    //     fwrite(record_buf + idx, NB_RECORD, 1, out);
    // }
    pwrite(out->fd, record_buf + write_offset, (num_records - write_offset) * NB_RECORD, 0);

    buffered_flush(out);
    // stop_and_print_interval(&tin, "File write");
}

record_t *get_next_record(external_t &ext) {
    if (ext.remain[ext.cur] == 0) {
        // printf("%d before %d => ", ii, c);
        // for (int i = 0; i < k; i++) {
        //     printf("%d: %d ", i, exts[i].cur);
        // }
        // printf("\n");

        while (aio_error(&ext.aio) == EINPROGRESS);

        ssize_t readbytes = aio_return(&ext.aio);
        // if (readbytes % NB_RECORD != 0) {
        //     printf("%d\n", ext.aio.aio_offset);
        //     printf("%ld %d %d %d wrong\n", readbytes, errno, ext.bufsiz[ext.cur], aio_error(&ext.aio));
        // }
        if (readbytes <= 0) return NULL;
        ext.remain[ext.cur ^ 1] = readbytes / NB_RECORD;

        ext.aio.aio_buf = ext.buf[ext.cur];
        // ext.aio.aio_offset = ext
        ext.aio.aio_nbytes = ext.bufsiz[ext.cur] * NB_RECORD;
        ext.aio.aio_offset += readbytes;

        aio_read(&ext.aio);
        
        ext.cur ^= 1;
        ext.ptr = ext.buf[ext.cur];
        
        // assert(isSorted(ext.buf[ext.cur], ext.buf[ext.cur] + ext.bufsiz[ext.cur]));
        // printf("%d: first key ", c);
        // print_key(ext.buf[ext.cur]);

        // printf("after %d => ", c);
        // for (int i = 0; i < k; i++) {
        //     printf("%d: %d ", i, exts[i].cur);
        // }
        // printf("\n");
    }

    --ext.remain[ext.cur];
    ++ext.ptr;
    return ext.ptr - 1;
}
void kway_external_merge(external_t exts[], size_t k, buffered_io_fd *out) {
    record_t *record;
    std::priority_queue<heap_item_t, std::vector<heap_item_t>, heap_comparison> q;
    for (int i = 0; i < k; i++) {
        q.push({ get_next_record(exts[i]), i });
    }

    // int i = 0;
    // int prevK = q.top().k;
    record_t prev = *q.top().record;
    while (!q.empty()) {
        heap_item_t p = q.top();
        q.pop();
        // ++i;

        if (q.empty()) {
            buffered_append(out, p.record, sizeof(record_t));
            buffered_flush(out);
            size_t kk = p.k;
            size_t insize = exts[kk].num * NB_RECORD;
            int fd = exts[kk].file->fd;
            off_t inoff = exts[kk].aio.aio_offset, outoff = out->offset;
            outoff += pwrite(out->fd, exts[kk].ptr, exts[kk].remain[exts[kk].cur] * NB_RECORD, outoff);
            outoff += pwrite(out->fd, exts[kk].buf[exts[kk].cur ^ 1], exts[kk].remain[exts[kk].cur ^ 1] * NB_RECORD, outoff);

            ssize_t readbytes;
            while (readbytes > 0) {
                readbytes = pread(fd, record_buf, record_buf_size / NB_RECORD, inoff);
                if (readbytes <= 0) break;
                inoff += readbytes;
                outoff += pwrite(out->fd, record_buf, readbytes, outoff);
            }
            
            break;
        }

        // if (memcmp(&prev, p.record, NB_KEY) > 0) {
        //     printf("prevK: %d, curK: %d\n", prevK, p.k);
        //     printf("failed %d\n", i);
        //     print_key(&prev);
        //     print_key(p.record);
        // }
        
        buffered_append(out, p.record, sizeof(record_t));
        record = get_next_record(exts[p.k]);

        if (record != NULL) {
            q.push({ record, p.k });
        }

        // prev = *p.record;
        // prevK = p.k;
    }

    buffered_flush(out);
}

// void kway_external_merge(buffered_io_fd **tmpfiles, buffered_io_fd *out, size_t k) {
//     record_t *bufs[k], *ptrs[k], *record;
//     size_t sizs[k];
//     size_t rmsiz = record_buf_size, lsiz = 0;
//     ssize_t remains[k] = { 0, };
//     size_t max_bufsiz = record_buf_size / (NB_RECORD * k);
//     size_t bufsiz = record_buf_size / (NB_RECORD * k);
//     std::priority_queue<heap_item_t, std::vector<heap_item_t>, heap_comparison> q;
//     for (int i = k - 1; i >= 0; i--) {
//         if (i == 0) {
//             sizs[i] = rmsiz / NB_RECORD;
//         } else {
//             sizs[i] = min(get_filesize(tmpfiles[i]) / NB_RECORD, max_bufsiz);
//         }
        
//         // printf("%d %llu\n", i, sizs[i]);
//         bufs[i] = record_buf + lsiz;
//         lsiz += sizs[i];
//         rmsiz -= sizs[i] * NB_RECORD;
//         record = get_next_record(tmpfiles[i], bufs[i], &ptrs[i], sizs[i], &remains[i]);
//         q.push({ record, i });
//     }

//     while (!q.empty()) {
//         heap_item_t p = q.top();
//         q.pop();

//         if (q.empty()) {
//             buffered_append(out, p.record, sizeof(record_t));
//             buffered_flush(out);
//             size_t kk = p.k;
//             size_t insize = get_filesize(tmpfiles[kk]);
//             int fd = tmpfiles[kk]->fd;
//             off_t inoff = tmpfiles[kk]->offset, outoff = out->offset;
//             outoff += pwrite(out->fd, ptrs[kk], remains[kk] * NB_RECORD, outoff);

//             while (inoff < insize) {
//                 ssize_t readbytes = pread(fd, record_buf, record_buf_size / NB_RECORD, inoff);
//                 if (readbytes <= 0) break;
//                 inoff += readbytes;
//                 outoff += pwrite(out->fd, record_buf, readbytes, outoff);
//             }
            
//             break;
//         }
        
//         buffered_append(out, p.record, sizeof(record_t));
//         record = get_next_record(tmpfiles[p.k], bufs[p.k], &ptrs[p.k], bufsiz, &remains[p.k]);

//         if (record != NULL) {
//             q.push({ record, p.k });
//         }
//     }
//     buffered_flush(out);
// }

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("usage: %s <path to input> <path to output>\n", argv[0]);
        return 0;
    }

    input_fd = open(argv[1], O_RDONLY | O_NONBLOCK);
    if (input_fd == -1) {
        printf("error: cannot open file\n");
        return -1;
    }

    omp_set_num_threads(NUM_OF_THREADS);

    file_size = lseek(input_fd, 0, SEEK_END);
    total_records = file_size / NB_RECORD;

    record_buf_size = min(total_records * NB_RECORD, MAX_MEMSIZ_FOR_DATA);
    record_buf = (record_t*)malloc(record_buf_size);

    // outbuf_size = file_size > MAX_MEMSIZ_FOR_DATA ? OUTPUT_BUFSIZ : max(OUTPUT_BUFSIZ, MAX_MEMSIZ_FOR_DATA - file_size);
    outbuf = (byte*)malloc(OUTPUT_BUFSIZ);
    fout = buffered_open(argv[2], O_RDWR | O_CREAT | O_TRUNC | O_ASYNC, outbuf, OUTPUT_BUFSIZ);
    // fout = fopen(argv[2], "wb+");
    if (fout == NULL) {
        printf("error: cannot create output file\n");
        return -1;
    }

    pwrite(fout->fd, "\0", 1, file_size - 1);

    size_t num_record_for_partition = record_buf_size / NB_RECORD;
    size_t num_partition = total_records / num_record_for_partition + (total_records % num_record_for_partition != 0);

    if (num_partition <= 1) {
        // setvbuf(fout, outbuf, _IOFBF, OUTPUT_BUFSIZ);
        partial_sort(fout, 0, total_records, 0);
    } else {
        tmpfiles = (buffered_io_fd **)malloc(num_partition * sizeof(buffered_io_fd*));
        char name[15];
        for (int i = 0; i < num_partition; i++) {
            sprintf(name, TMPFILE_NAME, i);
            tmpfiles[i] = buffered_open(name, O_RDWR | O_CREAT | O_TRUNC, outbuf, OUTPUT_BUFSIZ);
            // tmpfiles[i] = fopen(name, "wb+");
        }

        external_t exts[num_partition];
        size_t max_bufsiz = record_buf_size / (NB_RECORD * num_partition);
        int tidx = num_partition - 1;
        size_t written = 0;
        for (off_t offset = (num_partition - 1) * num_record_for_partition; offset >= 0; offset -= num_record_for_partition, --tidx) {
            external_t &ext = exts[tidx];
            ext.file = tmpfiles[tidx];
            ext.num = min(offset + num_record_for_partition, total_records) - offset;
            off_t woff = tidx == 0 ? (written > ext.num ? max_bufsiz : max(max_bufsiz, ext.num - written)) : 0;
            partial_sort(tmpfiles[tidx], offset, ext.num, woff);
            written += ext.num;

            ext.remain[0] = woff / 2;
            ext.remain[1] = woff - ext.remain[0];
            memset(&ext.aio, 0, sizeof(struct aiocb));
            ext.aio.aio_fildes = ext.file->fd;

            buffered_reset(ext.file);
        }

        exts[0].buf[0] = exts[0].ptr = record_buf;
        exts[0].bufsiz[0] = exts[0].remain[0];
        exts[0].buf[1] = exts[0].buf[0] + exts[0].bufsiz[0];
        exts[0].bufsiz[1] = exts[0].remain[1];
        exts[0].cur = 0;
        exts[0].aio.aio_offset = -exts[0].remain[1] * NB_RECORD;
        exts[0].aio.__return_value = exts[0].remain[1] * NB_RECORD;
        assert(isSorted(exts[0].buf[0], exts[0].buf[1] + exts[0].bufsiz[1]));
        for (int i = 1; i < num_partition; i++) {
            size_t size = min(exts[i].num, max_bufsiz);
            exts[i].buf[0] = exts[i - 1].buf[1] + exts[i - 1].bufsiz[1];
            exts[i].bufsiz[0] = size / 2;
            exts[i].buf[1] = exts[i].buf[0] + exts[i].bufsiz[0];
            exts[i].bufsiz[1] = size - exts[i].bufsiz[0];
            exts[i].cur = 1;
            exts[i].remain[0] = exts[i].remain[1] = 0;
            
            exts[i].aio.aio_offset = 0;
            exts[i].aio.aio_buf = exts[i].buf[0];
            exts[i].aio.aio_nbytes = exts[i].bufsiz[0] * NB_RECORD;
            aio_read(&exts[i].aio);
        }

        kway_external_merge(exts, num_partition, fout);
    }

    close(input_fd);
    // time_interval_t tin;
    // begin_time_track(&tin);
    if (num_partition > 1) {
        char name[15];
        for (off_t i = 0; i < num_partition; i++) {
            sprintf(name, TMPFILE_NAME, i);
            buffered_close(tmpfiles[i]);
        }
        free(tmpfiles);
    }
    // stop_and_print_interval(&tin, "Flush File");

    // begin_time_track(&tin);
    buffered_close(fout);
    // stop_and_print_interval(&tin, "Flush File");

    free(outbuf);
    free(record_buf);

    return 0;
}