#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/time.h>       // for gettimeofday
#include <string.h>
#include <errno.h>

// Define the system call number if not present in your headers:
#ifndef SYS_app_helper
#define SYS_app_helper 449  // <--- Adjust to match your setup
#endif

// Default values
#define DEFAULT_BUF_SIZE       256
#define NUM_ITERATIONS 100000

// A helper function to get current time in microseconds
static long get_time_us(void) {
    struct timeval tv;
    if (gettimeofday(&tv, NULL) < 0) {
        perror("gettimeofday");
        exit(EXIT_FAILURE);
    }
    return (tv.tv_sec * 1000000L) + tv.tv_usec;
}

int main(int argc, char **argv) {
    char *buffer;
    int i;
    long start_time, end_time;
    long total_time = 0;
    size_t buf_size = DEFAULT_BUF_SIZE;

    // If a command-line argument is provided, use it as the buffer size.
    if (argc > 1) {
        buf_size = (size_t)atoi(argv[1]);
        if (buf_size == 0) {
            fprintf(stderr, "Invalid buffer size provided. Using default %d bytes.\n", DEFAULT_BUF_SIZE);
            buf_size = DEFAULT_BUF_SIZE;
        }
    }
    printf("Using buffer size: %zu bytes\n", buf_size);

    // Allocate user-level buffer
    buffer = (char *)malloc(buf_size);
    if (!buffer) {
        perror("malloc");
        return EXIT_FAILURE;
    }

    // Pre-run: Initialize buffer with 4
    memset(buffer, 4, buf_size);

    // Test single call before loop to ensure correctness
    if (syscall(SYS_app_helper, buffer, buf_size) < 0) {
        perror("syscall app_helper (test)");
        free(buffer);
        return EXIT_FAILURE;
    }
    // Validate that buffer is set to 1
    for (i = 0; i < buf_size; i++) {
        if (buffer[i] != 1) {
            fprintf(stderr, "Validation failed at index %d (expected 1, got %d)\n",
                    i, buffer[i]);
            free(buffer);
            return EXIT_FAILURE;
        }
    }
    // Reset buffer to 4 for the timed loop
    memset(buffer, 4, buf_size);

    // Benchmark loop
    start_time = get_time_us();
    for (i = 0; i < NUM_ITERATIONS; i++) {
        // Reinitialize buffer with 4 on each iteration
        memset(buffer, 4, buf_size);

        // Invoke the system call
        if (syscall(SYS_app_helper, buffer, buf_size) < 0) {
            perror("syscall app_helper (loop)");
            free(buffer);
            return EXIT_FAILURE;
        }
    }
    end_time = get_time_us();
    total_time = end_time - start_time;

    // Compute average time in microseconds
    double avg_time_us = (double)total_time / (double)NUM_ITERATIONS;

    printf("Ran %d iterations.\n", NUM_ITERATIONS);
    printf("Total time: %ld microseconds.\n", total_time);
    printf("Average time per syscall: %.2f microseconds.\n", avg_time_us);

    // Final validation to confirm buffer is set to 1
    for (i = 0; i < buf_size; i++) {
        if (buffer[i] != 1) {
            fprintf(stderr, "Final validation failed at index %d (expected 1, got %d)\n",
                    i, buffer[i]);
            free(buffer);
            return EXIT_FAILURE;
        }
    }
    printf("Final validation succeeded! The buffer is correctly set to 1.\n");

    free(buffer);
    return EXIT_SUCCESS;
}
