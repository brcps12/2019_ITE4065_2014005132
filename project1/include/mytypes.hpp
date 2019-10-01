#ifndef __MYTYPES_HPP
#define __MYTYPES_HPP

#include <sys/types.h>

#define KB ((size_t)1024)
#define MB ((size_t)1048576)
#define GB ((size_t)1073741824)

#define NB_KEY (10) 
#define NB_PAYLOAD (90)
#define NB_RECORD (100)

typedef u_int8_t byte;
typedef byte rec_payload_t[NB_PAYLOAD];
typedef byte rec_key_t[NB_KEY];
typedef unsigned long long llu;

typedef struct {
    rec_key_t key;
    rec_payload_t payload;
} record_t;

#endif
