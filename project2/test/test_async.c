#include <pthread.h>
#include <stdlib.h>
#include <test.h>

size_t buf_size = DEFAULT_BUF_SIZE;

void perform_oprs(int threads) {
    long start_time, end_time;
    long double total_time;
    int *extns, *pages, i;
    pthread_t *ptid = calloc(sizeof(pthread_t), threads);
    void **buffers = calloc(sizeof(void*), threads);
    extns = (int*)calloc(sizeof(int), 1);
    pages = (int*)calloc(sizeof(int), 1);
    for (i = 0; i < threads; i++) {
        buffers[i] = create_bufs();
    }
    start_time = get_time_us();
    init_extents();
    for (i = 0; i < threads; i++) {
        pthread_create(&ptid[i], NULL, &set_bufs, buffers[i]);
    }
    for (i = 0; i < threads; i++) {
        pthread_join(ptid[i], NULL);
    }
    free_extents(extns, pages);
    end_time = get_time_us();
    total_time = (end_time - start_time);
    print_output(*extns, *pages, total_time, threads, "threads");
    for (i = 0; i < threads; i++) {
        if (!buffers[i] || munmap(buffers[i], buf_size) != 0) {
            fprintf(stderr, "Error in buffer...");
            exit(EXIT_FAILURE);
        }
    }
    free(buffers);
    free(extns);
    free(pages);
    free(ptid);
}

int main(int argc, char **argv) {
    int threads;

    if (argc == 1) {
        p_usage(argv, "threads");
        exit(EXIT_FAILURE);
    } else if (argc > 1) {
        threads = atoi(argv[1]);
        if (threads == 0) {
            fprintf(stderr, "Invalid thread count provided. Using default %d threads.\n", MAX_PROCS);
            threads = MAX_PROCS;
        }

        if (argc > 2) {
            buf_size = (size_t)atoi(argv[2]);
            if (buf_size == 0) {
                fprintf(stderr, "Invalid buffer size provided. Using default %d bytes.\n", DEFAULT_BUF_SIZE);
            }
        }
    }
    printf("Using buffer size: %zu bytes and running on %d threads.\n", buf_size, threads);
    perform_oprs(threads);
    return EXIT_SUCCESS;
}
