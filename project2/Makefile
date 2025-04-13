CC = gcc

all: test.o etest.o

test.o: test.c
	$(CC) $^ -O0 -g -o $@ -lpthread

etest.o: test.c
	$(CC) $^ -O0 -g -DUSE_SYSCALL -o $@ -lpthread

clean:
	rm -f test.o etest.o
