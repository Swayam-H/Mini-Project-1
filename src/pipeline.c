#include "pipeline.h"
#include "execute.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

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

static void execute_pipeline_segment(const TokenStream *stream, size_t start, size_t end, const char *shell_home) {
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
                return;
            }
            cmd_idx++;
            cmd_start = i + 1;
        }
    }

    int prev_pipe_read = -1;
    pid_t *pids = malloc(cmd_count * sizeof(pid_t));
    bool in_pipeline = (cmd_count > 1);

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
        
        pids[i] = execute_single_command(&cmds[i], shell_home, pipe_in, pipe_out, in_pipeline);

        if (pipe_in != -1) close(pipe_in);
        if (pipe_out != -1) close(pipe_out);

        prev_pipe_read = pipefd[0];
    }

    for (int i = 0; i < cmd_count; i++) {
        if (pids[i] > 0) {
            waitpid(pids[i], NULL, 0);
        }
    }

    for (int i = 0; i < cmd_count; i++) {
        free_command(&cmds[i]);
    }
    free(cmds);
    free(pids);
}

int execute_token_stream(const TokenStream *stream, const char *shell_home) {
    if (!stream || stream->count <= 1) return 0;

    size_t end = 0;
    while (end < stream->count && 
           stream->tokens[end].type != TOK_EOF && 
           stream->tokens[end].type != OP_SEMI && 
           stream->tokens[end].type != OP_AMP) {
        end++;
    }

    if (end > 0) {
        execute_pipeline_segment(stream, 0, end, shell_home);
    }

    return 0;
}
