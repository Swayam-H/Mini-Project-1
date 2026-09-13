#include "builtins.h"
#include "jobs.h"
#include <stdio.h>
#include <stdlib.h>

int builtin_activities(int argc, char **argv) {
    if (argc > 1) { printf("activities: invalid syntax\n"); return -1; }
    (void)argc;
    (void)argv;

    for (int i = 0; i < g_job_table.count; i++) {
        Job *j = &g_job_table.jobs[i];
        if (!j->active) continue;

        bool any_alive = false;
        for (int p = 0; p < j->proc_count; p++) {
            if (j->procs[p].state == JOB_RUNNING || j->procs[p].state == JOB_STOPPED) {
                any_alive = true;
                break;
            }
        }

        if (!any_alive) {
            j->active = false; 
            continue;
        }

        printf("[%d] pgid %d\n", j->job_number, j->pgid);

        for (int p = 0; p < j->proc_count; p++) {
            if (j->procs[p].state == JOB_RUNNING) {
                printf("  %d %s Running\n", j->procs[p].pid, j->procs[p].cmd_name);
            } else if (j->procs[p].state == JOB_STOPPED) {
                printf("  %d %s Stopped\n", j->procs[p].pid, j->procs[p].cmd_name);
            }
        }
    }

    return 0;
}
