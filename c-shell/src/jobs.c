#include "jobs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>

JobTable g_job_table;
volatile sig_atomic_t g_child_changed = 0;

void job_table_init(void) {
    g_job_table.count = 0;
    g_job_table.next_job_number = 1; 
}

Job* job_add(pid_t pgid, const char *cmdline, bool is_bg) {
    if (g_job_table.count >= MAX_JOBS) {
        
        int w = 0;
        for (int r = 0; r < g_job_table.count; r++) {
            if (g_job_table.jobs[r].active) {
                if (w != r) g_job_table.jobs[w] = g_job_table.jobs[r];
                w++;
            }
        }
        g_job_table.count = w;
        if (g_job_table.count >= MAX_JOBS) return NULL;
    }
    
    Job *j = &g_job_table.jobs[g_job_table.count++];
    j->job_number = g_job_table.next_job_number++;
    j->pgid = pgid;
    j->proc_count = 0;
    strncpy(j->cmdline, cmdline, sizeof(j->cmdline) - 1);
    j->cmdline[sizeof(j->cmdline) - 1] = '\0';
    j->active = true;
    j->is_background = is_bg;
    j->reported = false;
    return j;
}

void job_add_process(Job *job, pid_t pid, const char *cmd_name) {
    if (job->proc_count >= MAX_PROCS_PER_JOB) return;
    Process *p = &job->procs[job->proc_count++];
    p->pid = pid;
    strncpy(p->cmd_name, cmd_name, sizeof(p->cmd_name) - 1);
    p->cmd_name[sizeof(p->cmd_name) - 1] = '\0';
    p->state = JOB_RUNNING;
}

Job* job_find_by_number(int job_number) {
    for (int i = 0; i < g_job_table.count; i++) {
        if (g_job_table.jobs[i].active && g_job_table.jobs[i].job_number == job_number) {
            return &g_job_table.jobs[i];
        }
    }
    return NULL;
}

Job* job_find_by_pid(pid_t pid) {
    for (int i = 0; i < g_job_table.count; i++) {
        if (!g_job_table.jobs[i].active) continue;
        for (int p = 0; p < g_job_table.jobs[i].proc_count; p++) {
            if (g_job_table.jobs[i].procs[p].pid == pid) {
                return &g_job_table.jobs[i];
            }
        }
    }
    return NULL;
}

Process* job_find_process(pid_t pid) {
    for (int i = 0; i < g_job_table.count; i++) {
        if (!g_job_table.jobs[i].active) continue;
        for (int p = 0; p < g_job_table.jobs[i].proc_count; p++) {
            if (g_job_table.jobs[i].procs[p].pid == pid) {
                return &g_job_table.jobs[i].procs[p];
            }
        }
    }
    return NULL;
}

void job_mark_process_state(pid_t pid, JobState state) {
    Process *p = job_find_process(pid);
    if (p) p->state = state;
}

bool job_has_stopped(void) {
    for (int i = 0; i < g_job_table.count; i++) {
        if (!g_job_table.jobs[i].active) continue;
        for (int p = 0; p < g_job_table.jobs[i].proc_count; p++) {
            if (g_job_table.jobs[i].procs[p].state == JOB_STOPPED) return true;
        }
    }
    return false;
}

void sigchld_handler(int sig) {
    (void)sig;
    int saved_errno = errno;
    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status)) {
            job_mark_process_state(pid, JOB_DONE); 
            g_child_changed = 1;
        } else if (WIFSIGNALED(status)) {
            job_mark_process_state(pid, JOB_KILLED); 
            g_child_changed = 1;
        }
    }
    errno = saved_errno;
}

void job_report_completed(void) {
    if (!g_child_changed) return;
    g_child_changed = 0;

    for (int i = 0; i < g_job_table.count; i++) {
        Job *j = &g_job_table.jobs[i];
        if (!j->active || !j->is_background) continue;

        bool all_done = true;
        for (int p = 0; p < j->proc_count; p++) {
            if (j->procs[p].state == JOB_RUNNING || j->procs[p].state == JOB_STOPPED) {
                all_done = false;
                break;
            }
        }

        if (all_done && !j->reported) {
            for (int p = 0; p < j->proc_count; p++) {
                if (j->procs[p].state == JOB_DONE) {
                    printf("%s with pid %d exited normally\n", j->procs[p].cmd_name, j->procs[p].pid);
                } else if (j->procs[p].state == JOB_KILLED) {
                    printf("%s with pid %d exited abnormally\n", j->procs[p].cmd_name, j->procs[p].pid);
                }
            }
            j->reported = true;
            j->active = false; 
        }
    }
}
