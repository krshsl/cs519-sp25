COMPILER_OPTS_1 = -march=native -floop-parallelize-all -Ofast -Wall -Werror
COMPILER_OPTS_2 = -march=native -floop-parallelize-all -Ofast -Wall -Werror
LIBRARY_OPTS = -lm -lc
CC = gcc

all: pipe shmem

pipe: IPC-pipe.c
	$(CC) IPC-pipe.c -o pipe $(LIBRARY_OPTS) $(COMPILER_OPTS_1)

shmem: IPC-shmem.c
	$(CC) IPC-shmem.c -o shmem $(LIBRARY_OPTS) $(COMPILER_OPTS_2)
clean:
	rm -f pipe shmem