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
    long double average = 0;
    for (int i = 0; i < ITERS; i++) {
        struct timespec start, end;
        DATA_TYPE *data = malloc(DATA_SIZE), *check = malloc(DATA_SIZE);
        if(data == NULL || check == NULL) {
            fprintf(stderr, "Error allocating memory: %s\n", strerror(errno));
            return -1;
        }
        memset(data, 4, DATA_SIZE);
        memset(check, 1, DATA_SIZE);

        clock_gettime(CLOCK_REALTIME, &start);
        if(syscall(SYS_APP_HELPER, data, DATA_SIZE) == -1) {
            fprintf(stderr, "Error calling syscall: %s\n", strerror(errno));
            free(data);
            free(check);
            return -1;
        }
        clock_gettime(CLOCK_REALTIME, &end);
        long timeElapsed = (end.tv_sec - start.tv_sec) * 1000000000L + (end.tv_nsec - start.tv_nsec);
        average += timeElapsed;
        // printf("Time taken: %ld nanoseconds\n", timeElapsed);
        if (memcmp(data, check, DATA_SIZE) == 0) {
            // printf("The system call returned the expected result\n");
        } else {
            printf("The system call did not return the expected result\n");
        }
        free(data);
        free(check);
    }

    printf("Average Time taken: %Lf nanoseconds\n", average/ITERS);
    return (int) 1;
}

int main(void) {
    testcall();
    return 0;
}