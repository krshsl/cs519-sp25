#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#define SYS_APP_HELPER 335
#define DATA_TYPE char
#define DATA_SIZE 256*sizeof(DATA_TYPE)
#define ITERS 1000000

#include <stdio.h>
#include <sys/time.h>

long long current_timestamp_micros() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000LL + tv.tv_usec;
}

int testcall() {
    long long start_time, end_time;
    long double averageTime = 0;
    long success, failure, iter;
    success = failure = 0;
    for (iter = 0; iter < ITERS; iter++) {
        struct timespec start, end;
        DATA_TYPE *data = malloc(DATA_SIZE), *check = malloc(DATA_SIZE);
        
        if(data == NULL || check == NULL) break;
        memset(data, 4, DATA_SIZE);
        memset(check, 1, DATA_SIZE);
        start_time = current_timestamp_micros();
        
        if(syscall(SYS_APP_HELPER, data, DATA_SIZE) == -1) {
            free(data);
            free(check);
            failure++;
            continue;
        }
        end_time = current_timestamp_micros();
        averageTime += end_time - start_time;
        
        if (memcmp(data, check, DATA_SIZE) == 0) {
            success++;
        } else {
            failure++;
        }
        free(data);
        free(check);
    }

    printf("Total Iters: %ld, Success Iters: %ld, Failure Iters: %ld\n", iter, success, failure);
    printf("Success Rate: %f, Failure Rate: %f\n", (double)success/iter, (double)failure/iter);
    printf("Average Time taken: %Lf microseconds\n", averageTime/iter);
    return (int) 1;
}

int main(void) {
    testcall();
    return 0;
}