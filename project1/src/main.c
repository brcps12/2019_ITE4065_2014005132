#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <memory.h>
#include <assert.h>
#include <string.h>
// #include <algorithm>

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

#define KB ((size_t)(1024))
#define MB ((size_t)(1024 * KB))
#define GB ((size_t)(1024 * MB))

#define NB_KEY (10) 
#define NB_PAYLOAD (90)
#define NB_RECORD (100)

#define RECORD_THRESHOLD 200000

#define NUM_OF_THREADS (80)
// It can be set in dynamically: currently 80% of total(=2g)
#define MAX_MEMSIZ_FOR_DATA ((size_t)(0.76 * 2 * GB))
#define MAX_RECORD_NUM ((size_t)(MEMSIZ_FOR_DATA / NB_RECORD))
#define INPUT_BUFSIZ (64 * MB)
#define OUTPUT_BUFSIZ (64 * MB)

#define TMPFILE_NAME "tmp.%d"

// type definitions
typedef char byte;
typedef byte rec_payload_t[NB_PAYLOAD];
typedef byte rec_key_t[NB_KEY];
typedef unsigned long long llu;

typedef struct {
    rec_key_t key;
    rec_payload_t payload;
} record_t;

typedef struct {
    rec_key_t key;
    off_t idx;
} comp_record_t;

typedef struct {
    off_t start_idx;
    record_t *buf;
} record_buffer_t;

FILE *fin;
FILE *fout;

size_t record_buf_size;
size_t file_size, total_records;

byte *inbuf[2];
byte *outbuf;
record_t *record_buf;
off_t *record_offs;

FILE **tmpfiles;

int compare_record(const record_t *a, const record_t *b) {
    return memcmp(a, b, NB_KEY);
}

int compare(const off_t *a, const off_t *b) {
    return compare_record(record_buf + *a, record_buf + *b);
}

// int compare_for_sort(const off_t &a, const off_t &b) {
//     return compare_record(record_buf + a, record_buf + b) < 0;
// }

// int compare_record_for_sort(const record_t &a, const record_t &b) {
//     return compare_record(&a, &b) < 0;
// }

void print_key(record_t *record) {
    char buf[22] = {0, };
    char *str = "0123456789ABCDEF";
    for (int i = 0; i < NB_KEY; i++) {
        buf[2 * i + 1] = str[record->key[i] & 15];
        buf[2 * i] = str[(record->key[i] >> 4) & 15];
    }
    printf("%s\n", buf);
}

void print_records() {
    for (off_t i = 0; i < total_records; i++) {
        print_key(record_buf + i);
    }
}

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

void read_and_sort(off_t start, size_t len) {
    len = read_records(fin, record_buf + start, len);
    for (off_t i = start; i < start + len; i++) {
        record_offs[i] = i;
    }
    // std::sort(record_offs + start, record_offs + start + len, compare_for_sort);
    // std::sort(record_buf + start, record_buf + start + len, compare_record_for_sort);
    // qsort(record_buf + start, len, sizeof(record_t), compare_record);
    qsort(record_offs + start, len, sizeof(off_t), compare);
    // partially_quicksort(record_buf, start, start + len - 1);
}

void twoway_merge(off_t *offin, off_t *offout, off_t start, off_t mid, off_t end) {
    off_t l = start, r = mid + 1, i = start;

    while (l <= mid && r <= end) {
        int res = compare(offin + l, offin + r);
        if (res < 0) {
            offout[i++] = offin[l++];
        } else {
            offout[i++] = offin[r++];
        }
    }

    while (l <= mid) {
        offout[i++] = offin[l++];
    }

    while (r <= end) {
        offout[i++] = offin[r++];
    }
}

void partial_sort(FILE *out, size_t num_records) {
    #pragma omp parallel for
    for (off_t start = 0; start < num_records; start += RECORD_THRESHOLD) {
        size_t len = start + RECORD_THRESHOLD > num_records ? num_records - start : RECORD_THRESHOLD;
        read_and_sort(start, len);
    }

    off_t *offin = record_offs, *offout = record_offs + num_records;
    for (size_t mlen = RECORD_THRESHOLD; mlen < num_records; mlen <<= 1) {
        #pragma omp parallel for
        for (off_t start = 0; start < num_records; start += (mlen << 1)) {
            off_t end = start + (mlen << 1) - 1;
            off_t mid = start + mlen - 1;
            if (mid >= num_records) {
                mid = end = num_records - 1;
            } else if (end >= num_records) {
                end = num_records - 1;
            }

            twoway_merge(offin, offout, start, mid, end);
        }

        swap((void**)&offin, (void**)&offout);
    }
    
    for (off_t i = 0; i < num_records; i ++) {
        off_t idx = offin[i];
        fwrite(record_buf + idx, NB_RECORD, 1, out);
    }

    fflush(out);
}

record_t *get_next_record(FILE *in, record_t *buf, record_t **ptr, size_t bufsiz, size_t *remain) {
    if (*remain == 0) {
        if (feof(in)) return NULL;
        *remain = read_records(in, buf, bufsiz / NB_RECORD);
        *ptr = buf;
    }

    --*remain;
    *ptr += 1;
    return *ptr - 1;
}

void external_merge(FILE *fin_left, FILE *fin_right, FILE *fout) {
    // simple
    size_t bufsiz = record_buf_size / (NB_RECORD * 2);
    size_t lremain = 0, rremain = 0;
    record_t *lbuf = record_buf, *rbuf = record_buf + bufsiz;
    record_t *lp = NULL, *rp = NULL;
    record_t *l = get_next_record(fin_left, lbuf, &lp, bufsiz, &lremain);
    record_t *r = get_next_record(fin_right, rbuf, &rp, bufsiz, &rremain);
    
    // record_t prev, cur;
    // size_t it = 0;
    while (l && r) {
        // prev = cur;
        if (compare_record(l, r) < 0) {
            fwrite(l, NB_RECORD, 1, fout);
            // cur = *l;
            l = get_next_record(fin_left, lbuf, &lp, bufsiz, &lremain);
        } else {
            fwrite(r, NB_RECORD, 1, fout);
            // cur = *r;
            r = get_next_record(fin_right, rbuf, &rp, bufsiz, &rremain);
        }
        // ++it;

        // if (it > 1) {
            // assert(compare_record(&prev, &cur) <= 0);
        // }
    }

    while (l) {
        // prev = cur;
        fwrite(l, NB_RECORD, 1, fout);
        // cur = *l;
        l = get_next_record(fin_left, lbuf, &lp, bufsiz, &lremain);
        // ++it;

        // if (it > 1) {
            // assert(compare_record(&prev, &cur) <= 0);
        // }
    }

    while (r) {
        // prev = cur;
        fwrite(r, NB_RECORD, 1, fout);
        // cur = *r;
        r = get_next_record(fin_right, rbuf, &rp, bufsiz, &rremain);
        // ++it;

        // if (it > 1) {
            // assert(compare_record(&prev, &cur) <= 0);
        // }
    }

    fflush(fout);
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("usage: %s <path to input> <path to output>\n", argv[0]);
        return 0;
    }

#ifdef LOCAL_TEST
    printf("This runs in local test only\n");
    char *num_thread = getenv("MP_NUM_OF_THREAD");
    omp_set_num_threads(atoi(num_thread));
#else
    omp_set_num_threads(NUM_OF_THREADS);
#endif

    inbuf[0] = (byte*)malloc(INPUT_BUFSIZ);
    inbuf[1] = (byte*)malloc(INPUT_BUFSIZ);
    outbuf = (byte*)malloc(OUTPUT_BUFSIZ);

    fin = fopen(argv[1], "rb");
    if (fin == NULL) {
        printf("error: cannot open file\n");
        return -1;
    }
    setvbuf(fin, inbuf[0], _IOFBF, INPUT_BUFSIZ);

    fseeko(fin, 0, SEEK_END);
    file_size = ftello(fin);
    rewind(fin);
    total_records = file_size / NB_RECORD;

    fout = fopen(argv[2], "wb+");
    if (fout == NULL) {
        printf("error: cannot create output file\n");
        return -1;
    }
    setvbuf(fout, outbuf, _IOFBF, OUTPUT_BUFSIZ);

    record_buf_size = total_records * NB_RECORD > MAX_MEMSIZ_FOR_DATA ? MAX_MEMSIZ_FOR_DATA : total_records * NB_RECORD;
    record_buf = (record_t*)malloc(record_buf_size);

    size_t num_record_for_partition = record_buf_size / NB_RECORD;
    size_t num_partition = total_records / num_record_for_partition;
    if (total_records % num_record_for_partition != 0) {
        ++num_partition;
    }
    record_offs = (off_t*)malloc(2 * num_record_for_partition * sizeof(off_t));

    if (num_partition <= 1) {
        partial_sort(fout, total_records);
    } else {
        tmpfiles = (FILE **)malloc(2 * num_partition * sizeof(FILE*));
        char name[15];
        for (int i = 0; i < 2 * num_partition; i++) {
            sprintf(name, TMPFILE_NAME, i);
            tmpfiles[i] = fopen(name, "wb+");
        }

        int tidx = 0;
        for (off_t offset = 0; offset < total_records; offset += num_record_for_partition, ++tidx) {
            size_t num_records = min(offset + num_record_for_partition, total_records) - offset;
            partial_sort(tmpfiles[tidx], num_records);
        }

        size_t partition = num_partition;
        int fpm[2][partition];
        int k = 0, l = 0;

        for (int i = 0; i < partition; i++) {
            fpm[0][i] = i, fpm[1][i] = i + partition;
        }

        while (partition) {
            int oi = 0;
            for (int i = partition - 1; i >= 0; i -= 2, ++oi) {
                FILE *pfin_left, *pfin_right, *pfout;

                if (i == 0) {
                    swap((void**)&fpm[k ^ 1][oi], (void**)&fpm[k][i]);
                    continue;
                }

                pfin_left = tmpfiles[fpm[k][i - 1]];
                pfin_right = tmpfiles[fpm[k][i]];

                if (partition == 2) {
                    pfout = fout;
                } else {
                    pfout = tmpfiles[fpm[k ^ 1][oi]];
                }

                // printf("partition: %d, i: %d, left_file: %d, right_file: %d, out_file: %d, oi: %d\n", partition, i, fpm[k][i - 1], fpm[k][i], fpm[k ^ 1][oi], oi);

                fseeko(pfin_left, 0, SEEK_END);
                fseeko(pfin_right, 0, SEEK_END);
                size_t len_left = ftello(pfin_left), len_right = ftello(pfin_right);
                
                rewind(pfin_left);
                rewind(pfin_right);
                rewind(pfout);

                setvbuf(pfin_left, inbuf[0], _IOFBF, INPUT_BUFSIZ);
                setvbuf(pfin_right, inbuf[1], _IOFBF, INPUT_BUFSIZ);
                setvbuf(pfout, outbuf, _IOFBF, OUTPUT_BUFSIZ);
                external_merge(pfin_left, pfin_right, pfout);
            }

            if (partition == 2) break;

            k ^= 1;
            partition = partition / 2 + (partition & 1);
        }
    }

    if (num_partition > 1) {
        char name[15];
        for (off_t i = 0; i < 2 * num_partition; i++) {
            sprintf(name, TMPFILE_NAME, i);
            fclose(tmpfiles[i]);
            remove(name);
        }
        free(tmpfiles);
    }

    fclose(fin);
    fclose(fout);

    free(inbuf[0]);
    free(inbuf[1]);
    free(outbuf);
    free(record_buf);
    free(record_offs);

    return 0;
}
