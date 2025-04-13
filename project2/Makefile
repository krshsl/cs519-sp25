CC = gcc

all: test.o etest.o

test.o: test.c
	$(CC) $^ -g -o $@ -lpthread

etest.o: test.c
	$(CC) $^ -g -DUSE_SYSCALL -o $@ -lpthread

clean:
	rm -f test
