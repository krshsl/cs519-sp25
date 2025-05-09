TEST_OPTS=-g
COMPILER_OPTS=-march=native -floop-parallelize-all -Ofast -Wall -Werror

CC=gcc

all: test.o shmem_1.o shmem_2.o shmem_3.o

test.o: test.c
	$(CC) $^ $(TEST_OPTS) -o $@

shmem_1.o: IPC-shmem.c
	$(CC) $^ -o $@ $(COMPILER_OPTS)

shmem_2.o: IPC-shmem.c
	$(CC) $^ -DDO_INACTIVE -o $@ $(COMPILER_OPTS)

shmem_3.o: IPC-shmem.c
	$(CC) $^ -DDO_YIELD -o $@ $(COMPILER_OPTS)

clean:
	rm test.o shmem*.o
