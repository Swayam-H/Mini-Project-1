#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>
#include <stdbool.h>
#include <signal.h>

#define MAX_JOBS 256
#define MAX_PROCS_PER_JOB 32

typedef enum {
    JOB_RUNNING,
    JOB_STOPPED,
    JOB_DONE,       
    JOB_KILLED      
} JobState;

typedef struct {
    pid_t    pid;
    char     cmd_name[256];
    JobState state;
} Process;

typedef struct {
    int       job_number;   
    pid_t     pgid;         
    Process   procs[MAX_PROCS_PER_JOB];
    int       proc_count;
    char      cmdline[1024];
    bool      active;
    bool      is_background;
    bool      reported;
} Job;

typedef struct {
    Job  jobs[MAX_JOBS];
    int  count;
    int  next_job_number;   
} JobTable;

extern JobTable g_job_table;

extern volatile sig_atomic_t g_child_changed;

void     job_table_init(void);
Job*     job_add(pid_t pgid, const char *cmdline, bool is_bg);
void     job_add_process(Job *job, pid_t pid, const char *cmd_name);
Job*     job_find_by_pid(pid_t pid);
Job*     job_find_by_number(int job_number);
Process* job_find_process(pid_t pid);
void     job_mark_process_state(pid_t pid, JobState state);
void     job_report_completed(void);
bool     job_has_stopped(void);

void     sigchld_handler(int sig);

#endif
