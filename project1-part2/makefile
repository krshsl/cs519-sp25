COMPILER_OPTS_1 = -march=native -floop-parallelize-all -Ofast -Wall -Werror
COMPILER_OPTS_2 = -march=native -floop-parallelize-all -Ofast -Wall -Werror
LIBRARY_OPTS = -lm -lc
CC = gcc

all: pipe.o shmem.o

pipe.o: IPC-pipe.c
	$(CC) $^ -o $@ $(LIBRARY_OPTS) $(COMPILER_OPTS_1)

shmem.o: IPC-shmem.c
	$(CC) $^ -o $@ $(LIBRARY_OPTS) $(COMPILER_OPTS_2)

clean:
	rm -f pipe.o shmem.o
