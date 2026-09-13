#include "pipeline.h"
#include "execute.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "jobs.h"

static bool parse_single_command(const TokenStream *stream, size_t start, size_t end, Command *cmd) {
    cmd->argc = 0;
    cmd->argv = malloc((end - start + 1) * sizeof(char *));
    cmd->input_files = malloc((end - start + 1) * sizeof(char *));
    cmd->input_count = 0;
    cmd->output_files = malloc((end - start + 1) * sizeof(char *));
    cmd->output_modes = malloc((end - start + 1) * sizeof(int));
    cmd->output_count = 0;

    for (size_t i = start; i < end; i++) {
        TokenType type = stream->tokens[i].type;

        if (type == WORD) {
            cmd->argv[cmd->argc++] = stream->tokens[i].value;
        } else if (type == OP_LT) {
            i++;
            if (i < end && stream->tokens[i].type == WORD) {
                const char *fname = stream->tokens[i].value;
                int test_fd = open(fname, O_RDONLY);
                if (test_fd == -1) {
                    printf("cshell: no such file or directory\n");
                    free(cmd->argv);
                    free(cmd->input_files);
                    free(cmd->output_files);
                    free(cmd->output_modes);
                    return false;
                }
                close(test_fd);
                cmd->input_files[cmd->input_count++] = (char *)fname;
            }
        } else if (type == OP_GT || type == OP_GTGT) {
            int is_append = (type == OP_GTGT) ? 1 : 0;
            i++;
            if (i < end && stream->tokens[i].type == WORD) {
                const char *fname = stream->tokens[i].value;
                int flags = O_WRONLY | O_CREAT | (is_append ? O_APPEND : O_TRUNC);
                int test_fd = open(fname, flags, 0644);
                if (test_fd == -1) {
                    printf("cshell: unable to create file for writing\n");
                    free(cmd->argv);
                    free(cmd->input_files);
                    free(cmd->output_files);
                    free(cmd->output_modes);
                    return false;
                }
                close(test_fd);
                cmd->output_files[cmd->output_count] = (char *)fname;
                cmd->output_modes[cmd->output_count] = is_append;
                cmd->output_count++;
            }
        }
    }

    cmd->argv[cmd->argc] = NULL;
    return true;
}

static void free_command(Command *cmd) {
    free(cmd->argv);
    free(cmd->input_files);
    free(cmd->output_files);
    free(cmd->output_modes);
}

static int execute_pipeline_segment(const TokenStream *stream, size_t start, size_t end, const char *shell_home, bool is_bg) {
    int cmd_count = 1;
    for (size_t i = start; i < end; i++) {
        if (stream->tokens[i].type == OP_PIPE) cmd_count++;
    }

    Command *cmds = malloc(cmd_count * sizeof(Command));
    size_t cmd_idx = 0;
    size_t cmd_start = start;
    for (size_t i = start; i <= end; i++) {
        if (i == end || stream->tokens[i].type == OP_PIPE) {
            if (!parse_single_command(stream, cmd_start, i, &cmds[cmd_idx])) {
                for (size_t j = 0; j < cmd_idx; j++) free_command(&cmds[j]);
                free(cmds);
                return 0;
            }
            cmd_idx++;
            cmd_start = i + 1;
        }
    }

    int prev_pipe_read = -1;
    pid_t *pids = malloc(cmd_count * sizeof(pid_t));
    bool in_pipeline = (cmd_count > 1);

    char cmdline[1024] = {0};
    for (size_t i = start; i < end; i++) {
        const char *val = stream->tokens[i].value;
        if (!val) {
            switch(stream->tokens[i].type) {
                case OP_LT: val = "<"; break;
                case OP_GT: val = ">"; break;
                case OP_GTGT: val = ">>"; break;
                case OP_PIPE: val = "|"; break;
                case OP_AMP: val = "&"; break;
                case OP_SEMI: val = ";"; break;
                default: val = ""; break;
            }
        }
        strncat(cmdline, val, sizeof(cmdline) - strlen(cmdline) - 1);
        if (i < end - 1) strncat(cmdline, " ", sizeof(cmdline) - strlen(cmdline) - 1);
    }
    Job *job = job_add(0, cmdline, is_bg);
    pid_t pgid = 0;

    for (int i = 0; i < cmd_count; i++) {
        int pipefd[2] = {-1, -1};
        if (i < cmd_count - 1) {
            if (pipe(pipefd) < 0) {
                perror("pipe");
                break;
            }
        }

        int pipe_in = prev_pipe_read;
        int pipe_out = pipefd[1];
        
        pids[i] = execute_single_command(&cmds[i], shell_home, pipe_in, pipe_out, in_pipeline, pgid, is_bg);

        if (pids[i] > 0) {
            if (i == 0) {
                pgid = pids[i];
                if (job) job->pgid = pgid;
            }
            if (job) job_add_process(job, pids[i], cmds[i].argv[0]);
        }

        if (pipe_in != -1) close(pipe_in);
        if (pipe_out != -1) close(pipe_out);

        prev_pipe_read = pipefd[0];
    }

    if (is_bg) {
        if (job && pids[0] != -1) {
            printf("[%d] %d\n", job->job_number, job->pgid);
        }
        int result = 0;
        if (cmd_count == 1 && pids[0] == -1) {
            result = -1;
            if (job) job->active = false; 
        }
        for (int i = 0; i < cmd_count; i++) free_command(&cmds[i]);
        free(cmds);
        free(pids);
        return result;
    }

    if (cmd_count > 0 && pids[0] != -1 && pgid != 0) {
        tcsetpgrp(STDIN_FILENO, pgid);
    }

    for (int i = 0; i < cmd_count; i++) {
        if (pids[i] > 0) {
            int status;
            pid_t w = waitpid(pids[i], &status, WUNTRACED);
            if (w > 0 && WIFSTOPPED(status)) {
                if (job) {
                    for (int p = 0; p < job->proc_count; p++) {
                        if (job->procs[p].state == JOB_RUNNING) {
                            job->procs[p].state = JOB_STOPPED;
                        }
                    }
                    printf("\n[%d] + Stopped %s\n", job->job_number, job->cmdline);
                }
                break;
            } else if (w > 0 && job) {
                if (WIFEXITED(status)) job_mark_process_state(w, JOB_DONE);
                else if (WIFSIGNALED(status)) job_mark_process_state(w, JOB_KILLED);
            }
        }
    }

    tcsetpgrp(STDIN_FILENO, getpgrp());

    job_report_completed();

    int result = 0;
    if (cmd_count == 1 && pids[0] == -1) {
        result = -1;
    }

    for (int i = 0; i < cmd_count; i++) {
        free_command(&cmds[i]);
    }
    free(cmds);
    free(pids);
    return result;
}

int execute_token_stream(const TokenStream *stream, const char *shell_home) {
    if (!stream || stream->count <= 1) return 0;

    size_t pos = 0;

    while (pos < stream->count && stream->tokens[pos].type != TOK_EOF) {
        size_t seg_start = pos;
        size_t seg_end = pos;
        TokenType delimiter = TOK_EOF;

        while (seg_end < stream->count) {
            TokenType t = stream->tokens[seg_end].type;
            if (t == OP_SEMI || t == OP_AMP || t == TOK_EOF) {
                delimiter = t;
                break;
            }
            seg_end++;
        }

        if (seg_end > seg_start) {
            bool is_bg = (delimiter == OP_AMP);
            int result = execute_pipeline_segment(stream, seg_start, seg_end, shell_home, is_bg);

            if (result == -1) {
                break;
            }
        }

        if (delimiter == OP_SEMI || delimiter == OP_AMP) {
            pos = seg_end + 1;
        } else {
            break;  
        }
    }
    return 0;
}
