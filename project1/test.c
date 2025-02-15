#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <string.h>
#include <errno.h>

#define SYS_APP_HELPER 335

int testcall() {
    if(syscall(SYS_APP_HELPER) == -1) {
        fprintf(stderr, "Error calling syscall: %s\n", strerror(errno));
        return -1;
    }
    return (int) 1;
};

int main(void) {
    printf("The return code from the helloworld system call is %d\n", testcall());  
    return 0;
}