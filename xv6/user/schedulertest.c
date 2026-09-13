#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main() {
    int i;
    int pid = fork();
    if(pid < 0){
        printf("fork failed\n");
        exit(1);
    }
    
    // CPU-bound loop to guarantee time slice exhaustion
    volatile int x = 0;
    for (i = 0; i < 2000000000; i++) {
        x = x + 1;
    }
    
    if(pid > 0){
        wait(0);
    }
    exit(0);
}
