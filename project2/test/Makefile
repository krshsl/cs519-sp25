N_COMP=-O0 -g -I.
SYS_COMP=$(N_COMP) -DUSE_SYSCALL
CC = gcc
OUTPUT=ast.o east.o st.o est.o

all: $(OUTPUT)

st.o: test_sync.c
	$(CC) $^ $(N_COMP) -o $@

est.o: test_sync.c
	$(CC) $^ $(SYS_COMP) -o $@

ast.o: test_async.c
	$(CC) $^ $(N_COMP) -o $@ -lpthread

east.o: test_async.c
	$(CC) $^ $(SYS_COMP) -o $@ -lpthread

clean:
	rm -f $(OUTPUT)
