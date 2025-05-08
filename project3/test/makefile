COMPILER_COMMANDS=-g
COMPILER_OPTS = -march=native -floop-parallelize-all -Ofast -Wall -Werror
# LIBRARY_OPS = -lc

CC=gcc

all: test.o shmem.o

test.o: test.c
	$(CC) $^ $(COMPILER_COMMANDS) -o $@

shmem.o: IPC-shmem.c
	$(CC) $^ -o $@ $(LIBRARY_OPS) $(COMPILER_OPTS)

clean:
	rm test.o shmem.o
