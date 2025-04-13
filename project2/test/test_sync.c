#include <stdlib.h>
#include <test.h>

void perform_oprs(int iter, size_t buf_size) {
    int *extns_cnt, *pages_cnt, extns, pages, i;
    long start_time, end_time;
    long double total_time;
    void *buffer;
    extns_cnt = (int*)malloc(sizeof(int));
    pages_cnt = (int*)malloc(sizeof(int));
    i = extns = pages = 0;
    total_time = 0;
    for (; i < iter; i++) {
        start_time = get_time_us();
        init_extents();
        buffer = create_bufs(&buf_size);
        free_extents(extns_cnt, pages_cnt);
        end_time = get_time_us();
        extns += *extns_cnt;
        pages += *pages_cnt;
        total_time += (end_time - start_time);

        if (!buffer || munmap(buffer, buf_size) != 0) {
            fprintf(stderr, "Error in buffer...");
            exit(EXIT_FAILURE);
        }
    }
    printf("Total time: %Lf microseconds for %d iteration\n", total_time, iter);
    printf("Average time: %Lf microseconds for %d iteration\n", total_time/iter, iter);
#ifdef USE_SYSCALL
    printf("For %d iterations: %d extents and %d pages were used in total\n", iter, extns, pages);
    printf("For %d iterations: %d extents and %d pages were used on average\n", iter, extns/iter, pages/iter);
#endif
    free(extns_cnt);
    free(pages_cnt);
}

int main(int argc, char **argv) {
    int iter;
    size_t buf_size = DEFAULT_BUF_SIZE;

    if (argc == 1) {
        p_usage(argv, "operations");
        exit(EXIT_FAILURE);
    } else if (argc > 1) {
        iter = atoi(argv[1]);
        if (iter == 0) {
            fprintf(stderr, "Invalid operations count provided.\n");
            exit(EXIT_FAILURE);
        }

        if (argc > 2) {
            buf_size = (size_t)atoi(argv[2]);
            if (buf_size == 0) {
                fprintf(stderr, "Invalid buffer size provided. Using default %d bytes.\n", DEFAULT_BUF_SIZE);
                buf_size = DEFAULT_BUF_SIZE;
            }
        }
    }
    printf("Using buffer size: %zu bytes and performing %d operations.\n", buf_size, iter);
    perform_oprs(iter, buf_size);
    return EXIT_SUCCESS;
}
