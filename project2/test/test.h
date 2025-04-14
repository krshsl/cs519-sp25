#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/time.h>

#define DEFAULT_BUF_SIZE    1048576
#define MAX_ITERS           1000
#define MAX_PROCS           8

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

static long get_time_us(void) {
    struct timeval tv;
    if (gettimeofday(&tv, NULL) < 0) {
        perror("gettimeofday");
        exit(EXIT_FAILURE);
    }
    return (tv.tv_sec * 1000000L) + tv.tv_usec;
}

extern size_t buf_size;

void *create_bufs(void) {
    char *buffer = (char *)mmap(NULL, buf_size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    if (buffer == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }
    return buffer;
}

void *set_bufs(void *arg) {
    memset(arg, 4, buf_size); // this should touch all pages...
}

void p_usage(char **argv, const char *opr) {
    printf("%s <number of %s> <buffer size in bytes>\n", argv[0], opr);
    printf("example: %s 5 4096\n", argv[0]);
}

void init_extents() {
#ifdef USE_SYSCALL
    if (syscall(sys_enable_extents) < 0) {
        perror("syscall enable_extents (test)");
        exit(EXIT_FAILURE);
    }
#endif
}

void free_extents(int *extns, int *pages) {
    *extns = 0;
    *pages = 0;

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
}

void print_output(int extns, int pages, long double total_time, int count, const char *opr) {
    printf("Total time: %Lf microseconds for %d %s\n", total_time, count, opr);
    printf("Average time: %Lf microseconds for %d %s\n", (total_time/count), count, opr);
#ifdef USE_SYSCALL
    printf("For %d %s: %d extents and %d pages were used in total\n", count, opr, extns, pages);
    printf("For %d %s: %d extents and %d pages were used on average\n", count, opr, (extns/count), (pages/count));
#endif
}
