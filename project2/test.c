#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
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
#define MAX_ITERS           1
#define MAX_PROCS           4

static long get_time_us(void) {
    struct timeval tv;
    if (gettimeofday(&tv, NULL) < 0) {
        perror("gettimeofday");
        exit(EXIT_FAILURE);
    }
    return (tv.tv_sec * 1000000L) + tv.tv_usec;
}

void *create_bufs(void *arg) {
    if (syscall(sys_print_pid) < 0) {
        perror("syscall sys_print_pid (test)");
        exit(EXIT_FAILURE);
    }

    size_t *buf_size = (size_t*)arg;
    char *buffer = (char *)malloc(*buf_size);
    if (!buffer) {
        perror("malloc");
        return NULL;
    }
    memset(buffer, 4, *buf_size); // this should touch all pages...
    memset(buffer, 1, *buf_size); // this should modify all pages...
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
    if (buffer) {
        free(buffer);
    } else {
        exit(EXIT_FAILURE);
    }
}

void repeat_oprs(int iter, size_t buf_size) {
    long start_time, end_time;
    long double total_time = 0;
    int extns_cnt, pages_cnt, extns, pages, i;
    extns_cnt = pages_cnt = i = 0;
    start_time = get_time_us();
    for (; i < iter; i++) {
        init_pages(buf_size, &extns, &pages);
        extns_cnt += extns;
        pages_cnt += pages;
    }
    end_time = get_time_us();
    total_time = (end_time - start_time)/iter;
    printf("Total time: %Lf microseconds for %d iteration\n", total_time, iter);
#ifdef USE_SYSCALL
    printf("For %d iterations: %d extents and %d pages were used in total\n", iter, extns_cnt, pages_cnt);
#endif
}

void multi_oprs(size_t buf_size) {
    long start_time, end_time;
    long double total_time = 0;
    int extns_cnt, pages_cnt, i;
    pthread_t ptid[MAX_PROCS];
    void **buffers = malloc(MAX_PROCS*sizeof(void*));
    memset(buffers, 0, MAX_PROCS*sizeof(void*));
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
        if (buffers[i]) {
            free(buffers[i]);
        }
    }
    free(buffers);
    end_time = get_time_us();
    total_time = end_time - start_time;
    printf("Total time: %Lf microseconds for %d threads\n", total_time, MAX_PROCS);
#ifdef USE_SYSCALL
    printf("For %d threads: %d extents and %d pages were used in total\n", MAX_PROCS, extns_cnt, pages_cnt);
#endif
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

    // repeat_oprs(1, buf_size);
    // repeat_oprs(MAX_ITERS, buf_size);
    multi_oprs(buf_size);

    return EXIT_SUCCESS;
}
