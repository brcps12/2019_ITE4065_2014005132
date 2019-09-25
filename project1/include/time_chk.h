#ifndef __TIME_CHK_H
#define __TIME_CHK_H

#include <sys/time.h>

typedef struct {
    struct timeval begin, end;
    double interval;
} time_interval_t;

void begin_time_track(time_interval_t *interval) {
    #ifdef LOCAL_TEST
    gettimeofday(&interval->begin, NULL);
    #endif
}

void stop_time_track(time_interval_t *interval) {
    #ifdef LOCAL_TEST
    gettimeofday(&interval->end, NULL);
    interval->interval = (double) (interval->end.tv_sec - interval->begin.tv_sec) * 1000 + (double) (interval->end.tv_usec - interval->begin.tv_usec) / 1000;
    #endif
}

void print_interval(time_interval_t *interval, const char *prefix) {
    #ifdef LOCAL_TEST
    printf("%s: %'.3fms\n", prefix, interval->interval);
    #endif
}

void print_interval(time_interval_t *interval) {
    #ifdef LOCAL_TEST
    print_interval(interval, "");
    #endif
}

void stop_and_print_interval(time_interval_t *interval, const char *prefix) {
    #ifdef LOCAL_TEST
    stop_time_track(interval);
    print_interval(interval, prefix);
    #endif
}

#endif
