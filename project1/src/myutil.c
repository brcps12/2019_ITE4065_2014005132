#include <myutil.h>
#include <stdio.h>

void print_key(record_t *record) {
    char buf[22] = {0, };
    const char *str = "0123456789ABCDEF";
    for (int i = 0; i < NB_KEY; i++) {
        buf[2 * i + 1] = str[record->key[i] & 15];
        buf[2 * i] = str[(record->key[i] >> 4) & 15];
    }
    printf("%s\n", buf);
}

void print_records(record_t *records, size_t num) {
    for (off_t i = 0; i < num; i++) {
        print_key(records + i);
    }
}
