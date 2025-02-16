#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define SYS_APP_HELPER 335
#define DATA_TYPE char
#define DATA_SIZE 1024*sizeof(DATA_TYPE)

int testcall() {
    DATA_TYPE *data = malloc(DATA_SIZE), *check = malloc(DATA_SIZE);
    if(data == NULL || check == NULL) {
        fprintf(stderr, "Error allocating memory: %s\n", strerror(errno));
        return -1;
    }
    memset(data, 4, DATA_SIZE);
    memset(check, 1, DATA_SIZE);

    if(syscall(SYS_APP_HELPER, data, DATA_SIZE) == -1) {
        fprintf(stderr, "Error calling syscall: %s\n", strerror(errno));
        free(data);
        return -1;
    }

    if (memcmp(data, check, DATA_SIZE) == 0) {
        printf("The system call returned the expected result\n");
    } else {
        printf("The system call did not return the expected result\n");
    }
    return (int) 1;
};

int main(void) {
    printf("The return code from the helloworld system call is %d\n", testcall());  
    return 0;
}