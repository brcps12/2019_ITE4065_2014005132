#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <queue>
#include <algorithm>
#include <vector>

#include <mytypes.hpp>
#include <bufio.hpp>
#include <myutil.hpp>
#include <radixsort.hpp>

#ifdef TIME_TRACKING
#include <time_chk.hpp>
#endif

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

#define THRESHOLD_READ (1000000)
#define NUM_OF_THREADS (80)

// It can be set in dynamically: currently 90% of total(=2g)
#define MAX_MEMSIZ_FOR_DATA ((size_t)(0.95 * 2 * GB))

// maxinum number of records can be used
#define MAX_RECORD_NUM ((size_t)(MEMSIZ_FOR_DATA / NB_RECORD))

#define OUTPUT_BUFSIZ (64 * MB)

#define TMPFILE_NAME "tmp.%d"

typedef struct {
    record_t *record;
    off_t k;
} heap_item_t;

typedef struct {
    buffered_io_fd *file;
    record_t *buf, *ptr;
    size_t bufsiz, num;
    ssize_t remain;
    int cur;
} external_info_t;

class heap_comparison {
public:
    bool operator() (const heap_item_t &a, const heap_item_t &b) {
        return memcmp(a.record, b.record, NB_KEY) > 0;
    }
};

typedef std::priority_queue<heap_item_t, std::vector<heap_item_t>, heap_comparison> record_heap;

int fin;
buffered_io_fd *fout;

size_t record_buf_size;
record_t *record_buf;

size_t file_size, total_records;
byte *outbuf;
buffered_io_fd **tmpfiles;

void phase1_sorting(buffered_io_fd *out, off_t offset, size_t num_records, size_t write_offset) {
    #pragma omp parallel for
    for (off_t start = 0; start < num_records; start += THRESHOLD_READ) {
        size_t maxlen = start + THRESHOLD_READ >= num_records ? num_records - start : THRESHOLD_READ;
        pread(fin, record_buf + start, maxlen * NB_RECORD, (offset + start) * NB_RECORD);
    }

    radixsort(record_buf, num_records, 0);
    pwrite(out->fd, record_buf + write_offset, (num_records - write_offset) * NB_RECORD, 0);
    buffered_flush(out);
}

record_t *get_next_record(external_info_t &ext) {
    if (ext.remain == 0) {
        ssize_t readbytes = buffered_read(ext.file, ext.buf, ext.bufsiz * NB_RECORD);
        if (readbytes <= 0) return NULL;
        ext.remain = readbytes / NB_RECORD;
        ext.ptr = ext.buf;
    }

    --ext.remain;
    ++ext.ptr;
    return ext.ptr - 1;
}

void kway_external_merge(external_info_t exts[], size_t k) {
    record_t *record;
    record_heap heap;
    for (int i = 0; i < k; i++) {
        heap.push({ get_next_record(exts[i]), i });
    }

    while (!heap.empty()) {
        heap_item_t item = heap.top();
        heap.pop();

        if (heap.empty()) {
            buffered_append(fout, item.record, sizeof(record_t));
            buffered_flush(fout);
            int fd = exts[item.k].file->fd;
            off_t inoff = exts[item.k].file->offset, outoff = fout->offset;
            outoff += pwrite(fout->fd, exts[item.k].ptr, exts[item.k].remain * NB_RECORD, outoff);

            ssize_t readbytes = pread(fd, record_buf, record_buf_size / NB_RECORD, inoff);
            while (readbytes > 0) {
                inoff += readbytes;
                outoff += pwrite(fout->fd, record_buf, readbytes, outoff);
            }
            
            break;
        }

        buffered_append(fout, item.record, sizeof(record_t));
        record = get_next_record(exts[item.k]);

        if (record != NULL) {
            heap.push({ record, item.k });
        }
    }

    buffered_flush(fout);
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("usage: %s <path to input> <path to output>\n", argv[0]);
        return 0;
    }

    fin = open(argv[1], O_RDONLY);
    if (fin == -1) {
        printf("error: cannot open file\n");
        return -1;
    }

    omp_set_num_threads(NUM_OF_THREADS);

    file_size = lseek(fin, 0, SEEK_END);
    total_records = file_size / NB_RECORD;

    record_buf_size = min(total_records * NB_RECORD, MAX_MEMSIZ_FOR_DATA);
    record_buf = (record_t*)malloc(record_buf_size);

    outbuf = (byte*)malloc(OUTPUT_BUFSIZ);
    fout = buffered_open(argv[2], O_RDWR | O_CREAT | O_TRUNC | O_ASYNC, outbuf, OUTPUT_BUFSIZ);
    if (fout == NULL) {
        printf("error: cannot create output file\n");
        return -1;
    }

    // fill zero in output file
    pwrite(fout->fd, "\0", 1, file_size - 1);

    size_t num_record_for_partition = record_buf_size / NB_RECORD;
    size_t num_partition = total_records / num_record_for_partition + (total_records % num_record_for_partition != 0);

    if (num_partition <= 1) {
        phase1_sorting(fout, 0, total_records, 0);
    } else {
        tmpfiles = (buffered_io_fd **)malloc(num_partition * sizeof(buffered_io_fd*));
        char name[15];
        for (int i = 0; i < num_partition; i++) {
            sprintf(name, TMPFILE_NAME, i);
            tmpfiles[i] = buffered_open(name, O_RDWR | O_CREAT | O_TRUNC, outbuf, OUTPUT_BUFSIZ);
        }

        external_info_t exts[num_partition];
        size_t max_bufsiz = record_buf_size / (NB_RECORD * num_partition);
        size_t rmsize = record_buf_size / NB_RECORD;
        int tidx = num_partition - 1;
        size_t written = 0;
        for (off_t offset = (num_partition - 1) * num_record_for_partition; offset >= 0; offset -= num_record_for_partition, --tidx) {
            external_info_t &ext = exts[tidx];
            ext.file = tmpfiles[tidx];
            ext.num = min(offset + num_record_for_partition, total_records) - offset;
            rmsize -= tidx == 0 ? 0 : min(ext.num, max_bufsiz);
            off_t woff = tidx == 0 ? rmsize : 0;
            phase1_sorting(tmpfiles[tidx], offset, ext.num, woff);
            written += ext.num;
            ext.remain = woff;
            buffered_reset(ext.file);
        }
        exts[0].buf = exts[0].ptr = record_buf;
        exts[0].bufsiz = exts[0].remain;
        for (int i = 1; i < num_partition; i++) {
            exts[i].buf = exts[i - 1].buf + exts[i - 1].bufsiz;
            exts[i].bufsiz = min(exts[i].num, max_bufsiz);
        }

        kway_external_merge(exts, num_partition);
    }

    if (num_partition > 1) {
        for (off_t i = 0; i < num_partition; i++) {
            buffered_close(tmpfiles[i]);
        }
        free(tmpfiles);
    }

    close(fin);
    buffered_close(fout);
    free(outbuf);
    free(record_buf);

    return 0;
}