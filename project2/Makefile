CC = gcc

all: test.o etest.o

test.o: test.c
	$(CC) $^ -o $@

etest.o: test.c
	$(CC) $^ -DUSE_SYSCALL -o $@

clean:
	rm -f test
