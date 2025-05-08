COMPILER_COMMANDS=-g

CC=gcc

all: test.o

test.o: test.c
	$(CC) $^ $(COMPILER_COMMANDS) -o $@

clean:
	rm test.o
