#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <memory.h>
#include <assert.h>

// type definitions
typedef unsigned char byte;
typedef unsigned long long llu;

#define NB_KEY 10
#define NB_PAYLOAD 90
#define NB_RECORD 100 // NB_KEY + NB_PAYLOAD

#define RECORD_THRESHOLD 200000LL

#define NUM_OF_THREADS 80

#define MEMSIZ 1 * 1024 * 1024 * 1024 // 1 GB

typedef struct record_t {
    byte key[NB_KEY];
    off_t offset
} record_t;

void swap(record_t *a, record_t *b) {
    record_t t = *a;
    *a = *b;
    *b = t;
}

int compare(record_t *a, record_t *b) {
    return memcmp(a, b, NB_KEY);

    // llu *ak = (llu*)(a->key), *bk = (llu*)(b->key);
    // short *as = (short*)(a->key + 8), *bs = (short*)(b->key + 8);
    // if (*ak < *bk) {
    //     return -1;
    // } else if (*ak > *bk) {
    //     return 1;
    // } else {
    //     if (*as < *bs) {
    //         return -1;
    //     } else if (*as > *bs) {
    //         return 1;
    //     }
    // }

    // return 0;
}

size_t read_records(int input_fd, off_t offset, record_t *buf, size_t records) {
    for (off_t i = offset, k = 0; i < offset + records * NB_RECORD; i += NB_RECORD, k++) {
        if (pread(input_fd, buf + k, NB_KEY, i) == 0) {
            return k;
        }

        (buf + k)->offset = i / NB_RECORD;
    }

    return records;
}

size_t print_key(record_t *record) {
    char buf[22] = {0, };
    char *str = "0123456789ABCDEF";
    for (int i = 0; i < NB_KEY; i++) {
        buf[2 * i + 1] = str[record->key[i] & 15];
        buf[2 * i] = str[(record->key[i] >> 4) & 15];
    }
    printf("%s\n", buf);
}

// void merge_inmemory(off_t start, off_t mid, off_t end, data_type *records) {
//     off_t l = start, r = mid + 1, k = 0;
//     data_type *buf = (data_type*)malloc((end - start + 1) * sizeof(data_type));
//     while (l <= mid && r <= end) {
//         int res = cmp_record(records + l, records + r);
//         if (res < 0) {
//             buf[k] = records[l];
//             ++l;
//         } else {
//             buf[k] = records[r];
//             ++r;
//         }
//         ++k;
//     }

//     while (l <= mid) {
//         buf[k] = records[l];
//         ++l, ++k;
//     }

//     while (r <= end) {
//         buf[k] = records[r];
//         ++r, ++k;
//     }

//     memcpy(records + start, buf, (end - start + 1) * LEN_RECORD);
//     free(buf);
// }

void partially_partition(record_t *records, off_t start, off_t end, off_t *i, off_t *j) {
    if (end - start <= 1) {
        if (compare(&records[start], &records[end]) > 0) {
            swap(&records[start], &records[end]);
        }

        *i = start,
        *j = end;
        return;
    }

    off_t it = start;
    record_t pivot = records[start]; // todo: optimize

    while (it <= end) {
        int cmp = compare(&records[it], &pivot);
        if (cmp < 0) {
            swap(&records[start], &records[it]);
            ++start, ++it;
        } else if (cmp == 0) {
            ++it;
        } else {
            swap(&records[it], &records[end]);
            --end;
        }
    }

    *i = start - 1;
    *j = it;
}

void partially_quicksort(record_t *records, off_t start, off_t end) {
    if (start >= end) return;

    off_t i, j; 
    
    partially_partition(records, start, end, &i, &j);

    partially_quicksort(records, start, i);
    partially_quicksort(records, j, end);
}

void partially_sort(byte *mem, int input_fd, off_t offset) {
    off_t record_offset = offset / NB_RECORD;
    record_t *buf = (record_t*)(mem + record_offset * sizeof(record_t));
    size_t records = read_records(input_fd, offset, buf, RECORD_THRESHOLD);
    
    partially_quicksort(buf, 0, records - 1);
}

void merge(byte *mem, byte *mem2, off_t start, off_t mid, off_t end) {
    record_t *in = (record_t*)mem, *out = (record_t*)mem2;
    
    off_t l = start, r = mid + 1, i = start;

    while (l <= mid && r <= end) {
        int res = compare(in + l, in + r);
        if (res < 0) {
            out[i++] = in[l++];
        } else {
            out[i++] = in[r++];
        }
    }

    while (l <= mid) {
        out[i++] = in[l++];
    }

    while (r <= end) {
        out[i++] = in[r++];
    }
}

int main(int argc, char* argv[]) {
    srand(time(NULL));

    if (argc < 3) {
        printf("usage: %s <path to input> <path to output>\n", argv[0]);
        return 0;
    }

    omp_set_num_threads(NUM_OF_THREADS);

    // #pragma omp parallel
    // initiate_thread(&global_state[omp_get_thread_num()]);
    
    // printf("The number of threads: %d\n", omp_get_num_threads());

    int input_fd = open(argv[1], O_RDONLY);
    if (input_fd == -1) {
        printf("error: cannot open file\n");
        return -1;
    }

    size_t file_size = lseek(input_fd, 0, SEEK_END);
    size_t num_records = file_size / NB_RECORD;

    int output_fd = open(argv[2], O_RDWR | O_CREAT | O_TRUNC, 0777);
    if (output_fd == -1) {
        printf("error: cannot create output file\n");
        return -1;
    }

    byte *mem[2];
    mem[0] = (byte*)malloc(num_records * sizeof(record_t));
    mem[1] = (byte*)malloc(num_records * sizeof(record_t));

    // set empty content (also used to save temporary data)
    // pwrite(output_fd, "\0", 1, file_size * 2 - 1);

    #pragma omp parallel for
    for (size_t offset = 0; offset < file_size; offset += RECORD_THRESHOLD * NB_RECORD) {
        partially_sort(mem[0], input_fd, offset);
    }

    // off_t base_offset = 0;
    // for (size_t mlen = RECORD_THRESHOLD << 1; mlen < num_records; mlen <<= 1) {
    //     base_offset ^= (size_t)file_size;
    // }

    int k = 0;
    for (size_t mlen = RECORD_THRESHOLD; mlen < num_records; mlen <<= 1, k ^= 1) {
        #pragma omp parallel for
        for (off_t start = 0; start < num_records; start += (mlen << 1)) {
            off_t end = num_records <= start + (mlen << 1) ? num_records - 1 : start + (mlen << 1) - 1;
            
            if (end - start < mlen) {
                memcpy(mem[k ^ 1] + start * sizeof(record_t), mem[k] + start * sizeof(record_t), (end - start + 1) * sizeof(record_t));
                continue;
            };

            merge(mem[k], mem[k ^ 1], start, start + mlen - 1, end);
        }
    }

    // for (size_t offset = 1; offset < num_records; offset++) {
    //     record_t *tt = mem2;
    //     assert(compare(tt + offset - 1, tt + offset) <= 0);
    // }

    // rearrage records
    record_t *records = (record_t*)mem[k];
    #pragma omp parallel for
    for (size_t i = 0; i < num_records; i++) {
        int fd = open(argv[2], O_RDWR);
        byte *buf = mem[k ^ 1] + omp_get_thread_num() * NB_RECORD;
        memcpy(buf, records[i].key, NB_KEY);
        pread(input_fd, buf + NB_KEY, NB_PAYLOAD, records[i].offset * NB_RECORD + NB_KEY);
        pwrite(fd, buf, NB_RECORD, i * NB_RECORD);
        close(fd);
    }
    
    free(mem[0]);
    free(mem[1]);
    close(input_fd);
    close(output_fd);

    return 0;
}
