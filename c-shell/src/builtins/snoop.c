#include "builtins.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

#ifdef __linux__
#include <sys/ptrace.h>
#include <sys/user.h>
#endif

typedef struct {
    int    syscall_num;
    int    count;
    double total_time;
    int    first_seen_order;
} SyscallStat;

static int compare_stats(const void *a, const void *b) {
    const SyscallStat *sa = (const SyscallStat *)a;
    const SyscallStat *sb = (const SyscallStat *)b;
    if (sa->count != sb->count) {
        return sb->count - sa->count; 
    }
    return sa->first_seen_order - sb->first_seen_order; 
}

static const char* lookup_syscall_name(int num) {
    static const char* names[400] = {
        [0] = "read", [1] = "write", [2] = "open", [3] = "close", [4] = "stat",
        [5] = "fstat", [6] = "lstat", [7] = "poll", [8] = "lseek", [9] = "mmap",
        [10] = "mprotect", [11] = "munmap", [12] = "brk", [13] = "rt_sigaction",
        [14] = "rt_sigprocmask", [15] = "rt_sigreturn", [16] = "ioctl",
        [17] = "pread64", [18] = "pwrite64", [19] = "readv", [20] = "writev",
        [21] = "access", [22] = "pipe", [23] = "select", [24] = "sched_yield",
        [25] = "mremap", [26] = "msync", [27] = "mincore", [28] = "madvise",
        [29] = "shmget", [30] = "shmat", [31] = "shmctl", [32] = "dup",
        [33] = "dup2", [34] = "pause", [35] = "nanosleep", [36] = "getitimer",
        [37] = "alarm", [38] = "setitimer", [39] = "getpid", [40] = "sendfile",
        [41] = "socket", [42] = "connect", [43] = "accept", [44] = "sendto",
        [45] = "recvfrom", [46] = "sendmsg", [47] = "recvmsg", [48] = "shutdown",
        [49] = "bind", [50] = "listen", [51] = "getsockname", [52] = "getpeername",
        [53] = "socketpair", [54] = "setsockopt", [55] = "getsockopt",
        [56] = "clone", [57] = "fork", [58] = "vfork", [59] = "execve",
        [60] = "exit", [61] = "wait4", [62] = "kill", [63] = "uname",
        [230] = "clock_nanosleep", [231] = "exit_group"
    };
    if (num >= 0 && num < 400 && names[num] != NULL) {
        return names[num];
    }
    return NULL;
}

int builtin_snoop(int argc, char **argv) {
#ifndef __linux__
    printf("snoop: unsupported on this platform\n");
    return -1;
#else
    if (argc < 2) {
        printf("snoop: invalid syntax\n");
        return -1;
    }

    pid_t tracee;

    if (strcmp(argv[1], "-p") == 0) {
        if (argc != 3) {
            printf("snoop: invalid syntax\n");
            return -1;
        }
        tracee = atoi(argv[2]);

        if (kill(tracee, 0) != 0) {
            printf("snoop: no such process\n");
            return -1;
        }

        if (ptrace(PTRACE_ATTACH, tracee, NULL, NULL) < 0) {
            printf("snoop: no such process\n");
            return -1;
        }
        waitpid(tracee, NULL, 0);

    } else {
        tracee = fork();
        if (tracee == 0) {
            ptrace(PTRACE_TRACEME, 0, NULL, NULL);
            execvp(argv[1], argv + 1);
            fprintf(stderr, "snoop: command not found\n");
            _exit(127);
        }
        waitpid(tracee, NULL, 0);
    }

    SyscallStat stats[512];
    int stat_count = 0;

    ptrace(PTRACE_SYSCALL, tracee, NULL, NULL);

    bool in_syscall = false;
    int current_syscall = -1;
    struct timespec entry_time;
    int order_counter = 0;

    while (1) {
        int status;
        pid_t w = waitpid(tracee, &status, 0);
        if (w < 0) break;

        if (WIFEXITED(status) || WIFSIGNALED(status)) break;

        if (WIFSTOPPED(status)) {
            struct user_regs_struct regs;
            ptrace(PTRACE_GETREGS, tracee, NULL, &regs);

            if (!in_syscall) {
                current_syscall = regs.orig_rax;
                
                SyscallStat *s = NULL;
                for (int i = 0; i < stat_count; i++) {
                    if (stats[i].syscall_num == current_syscall) {
                        s = &stats[i];
                        break;
                    }
                }
                if (!s && stat_count < 512) {
                    s = &stats[stat_count++];
                    s->syscall_num = current_syscall;
                    s->count = 0;
                    s->total_time = 0.0;
                    s->first_seen_order = order_counter++;
                }
                if (s) {
                    s->count++;
                }
                
                clock_gettime(CLOCK_MONOTONIC, &entry_time);
                in_syscall = true;
            } else {
                struct timespec exit_time;
                clock_gettime(CLOCK_MONOTONIC, &exit_time);
                double elapsed = (exit_time.tv_sec - entry_time.tv_sec) + 
                                 (exit_time.tv_nsec - entry_time.tv_nsec) / 1e9;

                SyscallStat *s = NULL;
                for (int i = 0; i < stat_count; i++) {
                    if (stats[i].syscall_num == current_syscall) {
                        s = &stats[i];
                        break;
                    }
                }
                if (s) {
                    s->total_time += elapsed;
                }
                in_syscall = false;
            }

            ptrace(PTRACE_SYSCALL, tracee, NULL, NULL);
        }

    }

    qsort(stats, stat_count, sizeof(SyscallStat), compare_stats);

    printf("%-20s %10s %10s\n", "syscall", "calls", "time");
    for (int i = 0; i < stat_count; i++) {
        const char *name = lookup_syscall_name(stats[i].syscall_num);
        char name_buf[32];
        if (!name) {
            snprintf(name_buf, sizeof(name_buf), "syscall_%d", stats[i].syscall_num);
            name = name_buf;
        }
        printf("%-20s %10d %8.3fs\n", name, stats[i].count, stats[i].total_time);
    }

    return 0;
#endif
}
