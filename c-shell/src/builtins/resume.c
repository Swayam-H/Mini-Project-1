#include "builtins.h"
#include "jobs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

Job *g_timeout_job = NULL;
volatile sig_atomic_t g_alarm_fired = 0;

void sigalrm_handler(int sig) {
    (void)sig;
    g_alarm_fired = 1;
    if (g_timeout_job) {
        kill(-g_timeout_job->pgid, SIGTERM);
    }
}

int builtin_resume(int argc, char **argv) {
    if (argc < 3) { printf("resume: invalid syntax\n"); return -1; }
    if (argv[1][0] != '%') { printf("resume: invalid syntax\n"); return -1; }

    int job_num = atoi(argv[1] + 1);
    Job *job = job_find_by_number(job_num);
    if (!job || !job->active) {
        printf("resume: no such job\n");
        return -1;
    }

    char *mode = argv[2];

    if (strcmp(mode, "fg") == 0) {
        int timeout = 0;
        if (argc >= 5 && strcmp(argv[3], "--timeout") == 0) {
            timeout = atoi(argv[4]);
            if (timeout <= 0) { printf("resume: invalid syntax\n"); return -1; }
        } else if (argc > 3) {
            printf("resume: invalid syntax\n"); return -1;
        }

        kill(-job->pgid, SIGCONT);
        
        job->group_state = JOB_RUNNING;
        for (int p = 0; p < job->proc_count; p++) {
            if (job->procs[p].state == JOB_STOPPED) {
                job->procs[p].state = JOB_RUNNING;
            }
        }

        printf("%s\n", job->cmdline);
        tcsetpgrp(STDIN_FILENO, job->pgid);

        if (timeout > 0) {
            struct sigaction sa;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = SA_RESTART;
            sa.sa_handler = sigalrm_handler;
            sigaction(SIGALRM, &sa, NULL);

            g_timeout_job = job;
            g_alarm_fired = 0;
            alarm(timeout);
        }

        int status;
        for (int p = 0; p < job->proc_count; p++) {
            if (job->procs[p].state != JOB_DONE && job->procs[p].state != JOB_KILLED) {
                pid_t w = waitpid(job->procs[p].pid, &status, WUNTRACED);
                if (w > 0) {
                    if (WIFSTOPPED(status)) {
                        job_mark_process_state(w, JOB_STOPPED);
                        job->group_state = JOB_STOPPED;
                        printf("\n[%d] + Stopped %s\n", job->job_number, job->cmdline);
                        break;
                    } else if (WIFEXITED(status)) {
                        job_mark_process_state(w, JOB_DONE);
                    } else if (WIFSIGNALED(status)) {
                        job_mark_process_state(w, JOB_KILLED);
                    }
                }
            }
        }

        if (timeout > 0) {
            alarm(0);
        }

        if (g_alarm_fired) {
            printf("resume: job timed out\n");
            job->active = false; 
            g_alarm_fired = 0;
        }

        tcsetpgrp(STDIN_FILENO, getpgrp());

    } else if (strcmp(mode, "bg") == 0) {
        if (argc > 3) { printf("resume: invalid syntax\n"); return -1; }

        kill(-job->pgid, SIGCONT);
        job->group_state = JOB_RUNNING;
        for (int p = 0; p < job->proc_count; p++) {
            if (job->procs[p].state == JOB_STOPPED) {
                job->procs[p].state = JOB_RUNNING;
            }
        }
        job->is_background = true;
        printf("[%d] + Running %s\n", job->job_number, job->cmdline);
    } else {
        printf("resume: invalid syntax\n");
        return -1;
    }

    return 0;
}
