#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#define SYS_APP_HELPER 335
#define DATA_TYPE char
#define DATA_SIZE 1024*sizeof(DATA_TYPE)
#define ITERS 1

int testcall() {
    long double averageTime = 0;
    long success, failure, iter;
    success = failure = 0;
    for (iter = 0; iter < ITERS; iter++) {
        struct timespec start, end;
        DATA_TYPE *data = malloc(DATA_SIZE), *check = malloc(DATA_SIZE);
        
        if(data == NULL || check == NULL) break;
        memset(data, 4, DATA_SIZE);
        memset(check, 1, DATA_SIZE);
        clock_gettime(CLOCK_REALTIME, &start);
        
        if(syscall(SYS_APP_HELPER, data, DATA_SIZE) == -1) {
            free(data);
            free(check);
            failure++;
            continue;
        }
        clock_gettime(CLOCK_REALTIME, &end);
        long timeElapsed = (end.tv_sec - start.tv_sec) * 1000000000L + (end.tv_nsec - start.tv_nsec);
        averageTime += timeElapsed;
        
        if (memcmp(data, check, DATA_SIZE) == 0) {
            success++;
        } else {
            failure++;
        }
        free(data);
        free(check);
    }

    printf("Total Iters: %l, Success Iters: %l, Failure Iters: %l\n", iter, success, failure);
    printf("Success Rate: %f, Failure Rate: %f\n", (double)success/iter, (double)failure/iter);
    printf("Average Time taken: %Lf nanoseconds\n", averageTime/iter);
    return (int) 1;
}

int main(void) {
    testcall();
    return 0;
}