#include "time_util.h"


/************************************************************************************************
 * @brief get the elapsed seconds since the start
 * @param start struct timeval captured earlier
 * @return seconds as double
 * @note taken from threads example1 CIS*3090 and modified a bit 
 ************************************************************************************************/
double calc_time(
    struct timeval start
) {

    struct timeval end; 
    gettimeofday(&end, NULL);

    long long startusec = (long long)start.tv_sec * 1000000LL + (long long)start.tv_usec;
    long long endusec = (long long)end.tv_sec * 1000000LL + (long long)end.tv_usec;

    return (double)(endusec - startusec) / 1000000.0;
}