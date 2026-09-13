#define _DEFAULT_SOURCE
#include <unistd.h>
#include "builtins.h"
#include "jobs.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

int builtin_ping(int argc, char **argv) {
    if (argc != 3) {
        printf("ping: invalid syntax\n");
        return -1;
    }

    char *target_str = argv[1];
    char *signal_str = argv[2];

    if (signal_str[0] == '\0') {
        printf("ping: invalid syntax\n");
        return -1;
    }
    for (int i = 0; signal_str[i]; i++) {
        if (signal_str[i] < '0' || signal_str[i] > '9') {
            printf("ping: invalid syntax\n");
            return -1;
        }
    }
    
    int signal_number = atoi(signal_str);
    int actual_signal = signal_number % 64;

    if (target_str[0] == '%') {
        int job_num = atoi(target_str + 1);
        Job *job = job_find_by_number(job_num);
        if (!job || !job->active) {
            printf("ping: no such process found\n");
            return -1;
        }
        kill(-job->pgid, actual_signal);
        printf("Sent signal %d to %s\n", signal_number, target_str);
    } else {
        pid_t pid = atoi(target_str);
        Process *proc = job_find_process(pid);
        if (!proc) {
            printf("ping: no such process found\n");
            return -1;
        }
        kill(pid, actual_signal);
        printf("Sent signal %d to %d\n", signal_number, pid);
    }
    
    usleep(20000);
    return 0;
}
