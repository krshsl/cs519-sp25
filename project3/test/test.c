// lscpu | egrep -i 'core.*:|socket'
// time -p ./test.o 0 15 100000000
#define _GNU_SOURCE
#include <err.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef sys_set_inactive
#define sys_set_inactive 335
#endif


int main(int argc, char *argv[]) {
    long num_processors = sysconf(_SC_NPROCESSORS_ONLN);
    int           parentCPU, children;
    pid_t *child_pids, w;
    cpu_set_t     set;
    unsigned int  nloops;
    int status;

    if (num_processors < 0) {
        perror("sysconf");
        return 1;
    }
    child_pids = malloc(num_processors*sizeof(pid_t));
    if (child_pids == NULL) {
        fprintf(stderr, "No memory available for child_pids\n");
        exit(EXIT_FAILURE);
    }
    memset(child_pids, 0, num_processors*sizeof(pid_t));

    if (argc != 4) {
        fprintf(stderr, "Usage: %s parent-cpu num-child num-loops\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }
    parentCPU = atoi(argv[1]);
    children = atoi(argv[2]);
    children = ((children == 0) ? num_processors : (children > 10000 ? 10000 : children));
    nloops = atoi(argv[3]);

    CPU_ZERO(&set);
    printf("%d\n", getpid());
    for (int i = 0, j = 0; i < children; i++, j = (j+1)%num_processors) {
        pid_t pid = fork();
        if (pid < 0) {
            err(EXIT_FAILURE, "fork");
        } else if (pid == 0) {
            // j = (j == parentCPU) ? (j+1)%num_processors : j;
            CPU_SET(j, &set);
            // CPU_SET(parentCPU, &set);
            if (sched_setaffinity(getpid(), sizeof(set), &set) == -1)
                err(EXIT_FAILURE, "sched_setaffinity");

            for (unsigned int k = 0; k < nloops; k++) getppid();

            exit(EXIT_SUCCESS);
        }
        child_pids[i] = pid;
    }

    // CPU_SET(parentCPU, &set);
    // if (sched_setaffinity(getpid(), sizeof(set), &set) == -1)
    //     err(EXIT_FAILURE, "sched_setaffinity");

    // for (unsigned int j = 0; j < nloops/2; j++) getppid();

    while(1) {
        for (int i = 0, j = 1; i < children && j != children; i++) {
            if (child_pids[i]) {
                printf("childid::%d\n", child_pids[i]);
                if (syscall(sys_set_inactive, child_pids[i]) < 0) {
                    perror("syscall set_inactive (test)");
                    exit(EXIT_FAILURE);
                }

                w = waitpid(child_pids[i], &status, WNOHANG);
                // w = waitpid(child_pids[i], &status, WUNTRACED | WCONTINUED);
                if (w == -1) {
                    perror("waitpid");
                    exit(EXIT_FAILURE);
                }
                printf("exited, status %d\n", WEXITSTATUS(status));
                printf("killed by signal %d\n", (WIFSIGNALED(status) && WTERMSIG(status)));
                printf("stopped by signal %d\n", WSTOPSIG(status));

                if (WEXITSTATUS(status) || (WIFSIGNALED(status) && WTERMSIG(status)) || WSTOPSIG(status)) {
                    child_pids[i] = 0;
                    j++;
                }
            }
        }
        sleep(1);
    }

    free(child_pids);
    exit(EXIT_SUCCESS);
}
