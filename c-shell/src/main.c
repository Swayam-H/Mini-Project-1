#include "prompt.h"
#include "lexer.h"
#include "parser.h"
#include "pipeline.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include "jobs.h"
#include "signals.h"
#include <signal.h>

int main(void) {
    char shell_home[PATH_MAX];

    if (getcwd(shell_home, sizeof(shell_home)) == NULL) {
        perror("getcwd failed");
        return 1;
    }

    job_table_init();
    install_shell_signals();

    char *line = NULL;
    size_t len = 0;
    ssize_t nread;
    bool ctrl_d_pending = false;

    while (1) {
        job_report_completed();
        display_prompt(shell_home);

        nread = getline(&line, &len, stdin);
        if (nread == -1) {
           
            if (job_has_stopped()) {
                if (!ctrl_d_pending) {
                    printf("\ncshell: there are stopped jobs\n");
                    ctrl_d_pending = true;
                    clearerr(stdin); 
                    continue;
                }
            }
            printf("\n");
            break;
        }

        ctrl_d_pending = false; 

        if (nread > 0) {
            if (line[nread - 1] == '\n') {
                line[nread - 1] = '\0';
                nread--;
            }
            if (nread > 0 && line[nread - 1] == '\r') {
                line[nread - 1] = '\0';
            }
        }

        TokenStream *tokens = tokenize(line);
        if (!tokens || !validate_syntax(tokens)) {
            printf("cshell: invalid syntax\n");
            fflush(stdout);
        } else if (tokens->count > 1) {
            execute_token_stream(tokens, shell_home);
        }

        free_token_stream(tokens);
    }

    for (int i = 0; i < g_job_table.count; i++) {
        if (g_job_table.jobs[i].active && g_job_table.jobs[i].pgid > 0) {
            kill(-g_job_table.jobs[i].pgid, SIGHUP);
        }
    }

    free(line);
    return 0;
}