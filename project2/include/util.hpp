#ifndef _UTIL_HPP
#define _UTIL_HPP

#include <stdlib.h>

// pick an integer randomly between begin and end inclusive
int randomNumber(int begin, int end) {
    return (end - begin + 1) * ((double)rand() / RAND_MAX) + begin;
}

#endif
