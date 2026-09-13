#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main() {
    int i, j;
    // IO-bound process: does a tiny bit of work, then sleeps.
    // Because it voluntarily yields before exhausting its time slice, 
    // it should NOT be demoted to lower priority queues (should stay in Q0).
    for (i = 0; i < 20; i++) {
        // small work
        volatile int x = 0;
        for(j=0; j<10000; j++) { x++; }
        
        // voluntarily give up CPU
        pause(2);
    }
    exit(0);
}
