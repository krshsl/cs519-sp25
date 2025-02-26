#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>

#define SYS_APP_HELPER 335
#define DATA_TYPE char
#define DATA_SIZE 1024*sizeof(DATA_TYPE)
#define ITERS 1000000
#define THREADS 40

struct analysis {
    long double averageTime;
    long success;
    long failure;
    long iter;
    int threads;
};

long long current_timestamp_micros() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000LL + tv.tv_usec;
}

void print_analysis(int argc, char *argv[], struct analysis *a) {
    if (argc > 2 && strcmp(argv[2], "csv") == 0) {
        printf("%ld,%Lf,%Lf,", a->iter, (long double)a->success/a->iter, (long double)a->failure/a->iter);
        printf("%Lf,%s,%d\n", a->averageTime/a->iter, argv[1], a->threads);
    } else {
        printf("Total Iters: %ld, Success Iters: %ld, Failure Iters: %ld\n", a->iter, a->success, a->failure);
        printf("Success Rate: %f, Failure Rate: %f\n", (double)a->success/a->iter, (double)a->failure/a->iter);
        printf("Average Time taken: %Lf microseconds\n", a->averageTime/a->iter);
    }
}

void perform_test(struct analysis **aPtr) {
    struct analysis *a = *aPtr;
    long long start_time, end_time;
    for (; a->iter < ITERS; a->iter++) {
        struct timespec start, end;
        DATA_TYPE *data = malloc(DATA_SIZE), *check = malloc(DATA_SIZE);
        
        if(data == NULL || check == NULL) break;
        memset(data, 4, DATA_SIZE);
        memset(check, 1, DATA_SIZE);
        start_time = current_timestamp_micros();
        
        if(syscall(SYS_APP_HELPER, data, DATA_SIZE) == -1) {
            free(data);
            free(check);
            a->failure++;
            continue;
        }
        end_time = current_timestamp_micros();
        a->averageTime += end_time - start_time;
        
        if (memcmp(data, check, DATA_SIZE) == 0) {
            a->success++;
        } else {
            a->failure++;
        }
        free(data);
        free(check);
    }
}

int testcall(int argc, char *argv[]) {
    struct analysis *a = malloc(sizeof(struct analysis));
    a->averageTime = a->success = a->iter = a->failure = 0L;
    a->threads = 0;
    perform_test(&a);
    print_analysis(argc, argv, a);    
    free(a);
    return (int) 1;
}

void *testcall_t(void *arg) {
    struct analysis *a = (struct analysis *)arg;
    perform_test(&a);
    return NULL;
}


int testcalls(int argc, char *argv[]) {
    pthread_t *thr;
    struct analysis *a = malloc(sizeof(struct analysis)*THREADS);
    
    for (int i = 0; i < THREADS; i++) {
        a[i].averageTime = a[i].success = a[i].iter = a[i].failure = 0L;
    }
    thr = calloc(sizeof(*thr), THREADS);

    for (int i = 0; i < THREADS; i++) {
        pthread_create(&thr[i], NULL, testcall_t, &a[i]);
    }

    for (int i = 0; i < THREADS; i++) {
        pthread_join(thr[i], NULL);
    }
    struct analysis *aF = malloc(sizeof(struct analysis));
    aF->averageTime = aF->success = aF->iter = aF->failure = 0L;
    aF->threads = THREADS;

    for (int i = 0; i < THREADS; i++) {
        aF->iter += a[i].iter;
        aF->success += a[i].success;
        aF->failure += a[i].failure;
        aF->averageTime += a[i].averageTime;
    }
    print_analysis(argc, argv, aF);
    free(aF);
    free(thr);
    free(a);
    return (int) 1;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <single/thread> [csv]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "single") == 0) {
        testcall(argc, argv);
    } else if (strcmp(argv[1], "thread") == 0) {
        testcalls(argc, argv);
    } else {
        printf("Invalid argument: %s\n", argv[1]);
        return 1;
    }
    return 0;
}