#ifndef _UTIL_HPP
#define _UTIL_HPP

#include <stdlib.h>
#include <random>
#include <ctime>
#include <functional>

// pick an integer randomly between begin and end inclusive
int randomNumber(int begin, int end) {
    thread_local std::mt19937 engine(std::random_device{}());
    std::uniform_int_distribution<int> dist(begin, end);

    return dist(engine);
}

#endif
