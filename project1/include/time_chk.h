#ifndef __TIME_CHK_H
#define __TIME_CHK_H

#include <sys/time.h>

typedef struct {
    struct timeval begin, end;
    double interval;
} time_interval_t;

void begin_time_track(time_interval_t *interval) {
    gettimeofday(&interval->begin, NULL);
}

void stop_time_track(time_interval_t *interval) {
    gettimeofday(&interval->end, NULL);
    interval->interval = (double) (interval->end.tv_sec - interval->begin.tv_sec) * 1000 + (double) (interval->end.tv_usec - interval->begin.tv_usec) / 1000;
}

void print_interval(time_interval_t *interval, const char *prefix) {
    printf("%s: %'.3fms\n", prefix, interval->interval);
}

void print_interval(time_interval_t *interval) {
    print_interval(interval, "");
}

void stop_and_print_interval(time_interval_t *interval, const char *prefix) {
    stop_time_track(interval);
    print_interval(interval, prefix);
}

#endif
