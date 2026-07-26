#ifndef TIME_UTIL_H
#define TIME_UTIL_H

#include <sys/time.h>
#include <stddef.h> 

double calc_time(struct timeval start);
static inline void time_now(struct timeval *tv) {
    gettimeofday(tv, NULL);
}

#endif
