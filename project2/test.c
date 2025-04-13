#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>

#ifndef sys_enable_extents
#define sys_enable_extents 335
#endif

#ifndef sys_disable_extents
#define sys_disable_extents 336
#endif

#ifndef sys_get_ex_count
#define sys_get_ex_count 337
#endif

#ifndef sys_print_pid
#define sys_print_pid 338
#endif

// #define DEFAULT_BUF_SIZE    1048576
#define DEFAULT_BUF_SIZE    409600
#define MAX_ITERS           10000
#define MAX_PROCS           16

static long get_time_us(void) {
    struct timeval tv;
    if (gettimeofday(&tv, NULL) < 0) {
        perror("gettimeofday");
        exit(EXIT_FAILURE);
    }
    return (tv.tv_sec * 1000000L) + tv.tv_usec;
}

void *create_bufs(void *arg) {
// #ifdef USE_SYSCALL
    // if (syscall(sys_print_pid) < 0) {
    //     perror("syscall sys_print_pid (test)");
    //     exit(EXIT_FAILURE);
    // }
// #endif
    size_t *buf_size = (size_t*)arg;
    char *buffer = (char *)mmap(NULL, *buf_size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    if (buffer == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }
    memset(buffer, 4, *buf_size); // this should touch all pages...
    return buffer;
}

void init_pages(size_t buf_size, int *extns, int *pages) {
    void *buffer;
    *extns = 0;
    *pages = 0;
#ifdef USE_SYSCALL
    if (syscall(sys_enable_extents) < 0) {
        perror("syscall enable_extents (test)");
        exit(EXIT_FAILURE);
    }
#endif
    buffer = create_bufs(&buf_size);
    memset(buffer, 1, buf_size); // this should modify all pages...
#ifdef USE_SYSCALL
    if (syscall(sys_get_ex_count, extns, pages) < 0) {
        perror("syscall disable_extents (test)");
        exit(EXIT_FAILURE);
    }

    if (syscall(sys_disable_extents) < 0) {
        perror("syscall disable_extents (test)");
        exit(EXIT_FAILURE);
    }
#endif
    if (buffer == NULL || munmap(buffer, buf_size) != 0) {
        perror("buffer or munmap");
        exit(EXIT_FAILURE);
    }
}

void repeat_oprs(int iter, size_t buf_size) {
    long start_time, end_time;
    long double total_time = 0;
    int extns_cnt, pages_cnt, *extns, *pages, i;
    extns_cnt = pages_cnt = i = 0;
    extns = (int*)malloc(sizeof(int));
    pages = (int*)malloc(sizeof(int));
    for (; i < iter; i++) {
        start_time = get_time_us();
        init_pages(buf_size, extns, pages);
        end_time = get_time_us();
        total_time += (end_time - start_time);
        // printf("%d::%d:%d\n", i, *extns, *pages);
        extns_cnt += *extns;
        pages_cnt += *pages;
    }
    total_time /= iter;
    free(extns);
    free(pages);
    printf("Total time: %Lf microseconds for %d iteration\n", total_time, iter);
#ifdef USE_SYSCALL
    printf("For %d iterations: %d extents and %d pages were used in total\n", iter, extns_cnt, pages_cnt);
#endif
}

void multi_oprs(size_t buf_size) {
    long start_time, end_time;
    long double total_time = 0;
    int *extns_cnt, *pages_cnt, i;
    pthread_t ptid[MAX_PROCS];
    void **buffers = malloc(MAX_PROCS*sizeof(void*));
    memset(buffers, 0, MAX_PROCS*sizeof(void*));
    extns_cnt = (int*)malloc(sizeof(int));
    pages_cnt = (int*)malloc(sizeof(int));
    start_time = get_time_us();
#ifdef USE_SYSCALL
    if (syscall(sys_enable_extents) < 0) {
        perror("syscall enable_extents (test)");
        exit(EXIT_FAILURE);
    }
#endif
    for (i = 0; i < MAX_PROCS; i++) {
        pthread_create(&ptid[i], NULL, &create_bufs, &buf_size);
    }
    for (i = 0; i < MAX_PROCS; i++) {
        pthread_join(ptid[i], &buffers[i]);
        if (buffers[i] == NULL) {
            printf("Some error occurred...\n");
        }
    }
#ifdef USE_SYSCALL
    if (syscall(sys_get_ex_count, extns_cnt, pages_cnt) < 0) {
        perror("syscall disable_extents (test)");
        exit(EXIT_FAILURE);
    }

    if (syscall(sys_disable_extents) < 0) {
        perror("syscall disable_extents (test)");
        exit(EXIT_FAILURE);
    }
#endif
    for (i = 0; i < MAX_PROCS; i++) {
        if (buffers[i] == NULL || munmap(buffers[i], buf_size) != 0) {
            perror("buffer or munmap");
            exit(EXIT_FAILURE);
        }
    }
    free(buffers);
    end_time = get_time_us();
    total_time = end_time - start_time;
    printf("Total time: %Lf microseconds for %d threads\n", total_time, MAX_PROCS);
#ifdef USE_SYSCALL
    printf("For %d threads: %d extents and %d pages were used in total\n", MAX_PROCS, *extns_cnt, *pages_cnt);
#endif
    free(extns_cnt);
    free(pages_cnt);
}

int main(int argc, char **argv) {
    int i;
    size_t buf_size = DEFAULT_BUF_SIZE;

    if (argc > 1) {
        buf_size = (size_t)atoi(argv[1]);
        if (buf_size == 0) {
            fprintf(stderr, "Invalid buffer size provided. Using default %d bytes.\n", DEFAULT_BUF_SIZE);
            buf_size = DEFAULT_BUF_SIZE;
        }
    }
    printf("Using buffer size: %zu bytes\n", buf_size);

    repeat_oprs(1, buf_size);
    repeat_oprs(MAX_ITERS, buf_size);
    multi_oprs(buf_size);

    return EXIT_SUCCESS;
}
