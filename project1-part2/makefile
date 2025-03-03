COMPILER_OPTS = -g -O3 -Wall -Werror
LIBRARY_OPTS = -lm -lc
CC = gcc

all: pipe shmem

pipe: IPC-pipe.c
	$(CC) IPC-pipe.c -o pipe $(LIBRARY_OPTS) $(COMPILER_OPTS)

shmem: IPC-shmem.c
	$(CC) IPC-shmem.c -o shmem $(LIBRARY_OPTS) $(COMPILER_OPTS)
clean:
	rm -f pipe shmem