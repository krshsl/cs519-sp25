// lscpu | egrep -i 'core.*:|socket'
// time -p ./test.o 2 16 1000000
// make; ./test.o 2 2 100 &;  sudo dmesg -we
#define _GNU_SOURCE
#include <err.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef sys_set_inactive_pid
#define sys_set_inactive_pid 336
#endif


int main(int argc, char *argv[]) {
    int max_processors = (int)sysconf(_SC_NPROCESSORS_ONLN);
    int num_processors;
    int  children;
    pid_t *child_pids, w;
    cpu_set_t     set;
    unsigned int  nloops;
    int status;

    if (max_processors < 0) {
        perror("sysconf");
        return 1;
    }

    if (argc != 4) {
        fprintf(stderr, "Usage: %s num-cores(1-%d) num-child num-loops\n",
                argv[0], max_processors);
        exit(EXIT_FAILURE);
    }
    num_processors = atoi(argv[1]);
    num_processors = ((num_processors == 0) ? 1 : (num_processors > max_processors ? max_processors : num_processors));
    children = atoi(argv[2]);
    children = ((children == 0) ? num_processors : (children > 10000 ? 10000 : children));
    nloops = atoi(argv[3]);
    printf("child::%d,  max_procs::%d\n", children, num_processors);

    child_pids = malloc(children*sizeof(pid_t));
    if (child_pids == NULL) {
        fprintf(stderr, "No memory available for child_pids\n");
        exit(EXIT_FAILURE);
    }
    memset(child_pids, 0, children*sizeof(pid_t));

    CPU_ZERO(&set);
    for (int i = 0, j = 0; i < children; i++, j = (j+1)%num_processors) {
        pid_t pid = fork();
        if (pid < 0) {
            err(EXIT_FAILURE, "fork");
        } else if (pid == 0) {
            CPU_SET(j, &set);
            if (sched_setaffinity(getpid(), sizeof(set), &set) == -1)
                err(EXIT_FAILURE, "sched_setaffinity");

            for (unsigned int k = 0; k < nloops; k++) getppid();

            exit(EXIT_SUCCESS);
        }
        child_pids[i] = pid;
    }

#ifdef DO_INACTIVE
    for (int i = 0, j = 0; i < children; i++, j = (j+1)%num_processors) {
        if (child_pids[i]) {
            // printf("do_sys_call... %d\n", child_pids[i]);
            if (syscall(sys_set_inactive_pid, child_pids[i], j, 60000LL) < 0) {
                syscall(sys_set_inactive_pid); // just to be safe....
                exit(EXIT_FAILURE);
            }
        }
    }
#endif

    for (int i = 0; i < children; i++) {
        if (child_pids[i]) {
            wait(NULL); //wait for them to complete...
        }
    }

    free(child_pids);
    exit(EXIT_SUCCESS);
}
