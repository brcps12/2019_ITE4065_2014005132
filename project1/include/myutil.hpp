#ifndef __MY_UTIL_HPP
#define __MY_UTIL_HPP

#include <mytypes.hpp>
#include <stdlib.h>

void print_key(record_t *record);

void print_records(record_t *records, size_t num);

int myrandom(int begin, int end);

#endif
