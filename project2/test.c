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

#define DEFAULT_BUF_SIZE    1048576

static long get_time_us(void) {
    struct timeval tv;
    if (gettimeofday(&tv, NULL) < 0) {
        perror("gettimeofday");
        exit(EXIT_FAILURE);
    }
    return (tv.tv_sec * 1000000L) + tv.tv_usec;
}

void init_pages(size_t buf_size) {
    if (syscall(sys_enable_extents) < 0) {
        perror("syscall enable_extents (test)");
        exit(EXIT_FAILURE);
    }

    char *buffer = (char *)malloc(buf_size);
    if (!buffer) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    memset(buffer, 4, buf_size); // this should touch all pages...

    if (syscall(sys_disable_extents) < 0) {
        perror("syscall disable_extents (test)");
        free(buffer);
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char **argv) {
    int i;
    long start_time, end_time;
    long total_time = 0;
    size_t buf_size;

    if (argc > 1) {
        buf_size = (size_t)atoi(argv[1]);
        if (buf_size == 0) {
            fprintf(stderr, "Invalid buffer size provided. Using default %d bytes.\n", DEFAULT_BUF_SIZE);
            buf_size = DEFAULT_BUF_SIZE;
        }
    }
    printf("Using buffer size: %zu bytes\n", buf_size);
    
    start_time = get_time_us();
    init_pages(buf_size);
    end_time = get_time_us();
    total_time = end_time - start_time;
    printf("Total time: %ld microseconds.\n", total_time);
    return EXIT_SUCCESS;
}
