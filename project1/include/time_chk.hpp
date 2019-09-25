#ifndef __TIME_CHK_HPP
#define __TIME_CHK_HPP

#include <sys/time.h>

class TimeTracker {
private:
    struct timeval t_begin, t_end;
    double interval;
public:
    void start() {
        gettimeofday(&t_begin, NULL);
    }

    void stop() {
        gettimeofday(&t_end, NULL);
        interval = (double) (t_end.tv_sec - t_begin.tv_sec) * 1000 + (double) (t_end.tv_usec - t_begin.tv_usec) / 1000;
    }

    void print(const char *prefix) {
        printf("%s: %'.3fms\n", prefix, interval);
    }

    void stopAndPrint(const char *prefix) {
        stop();
        print(prefix);
    }

    void print() {
        print("");
    }
};

#endif
