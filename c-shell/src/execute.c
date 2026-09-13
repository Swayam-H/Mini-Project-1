#include "execute.h"
#include "builtins.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>

#define MAX_PATH_BUF (PATH_MAX * 2)
#define STREAM_CHUNK_SIZE 4096

static bool is_executable(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return false;
    if (!S_ISREG(st.st_mode)) return false;
    return (access(path, X_OK) == 0);
}

static void safe_join_path(char *dest, size_t size, const char *dir, const char *name) {
    dest[0] = '\0';
    strncat(dest, dir, size - 1);
    size_t len = strlen(dest);
    if (len + 1 < size && (len == 0 || dest[len - 1] != '/')) {
        strncat(dest, "/", size - len - 1);
    }
    len = strlen(dest);
    if (len < size) {
        strncat(dest, name, size - len - 1);
    }
}

static bool resolve_executable_path(const char *cmd, char *resolved_path, size_t max_len) {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) return false;

    bool skip_cwd = false;
    const char *lookup_name = cmd;

    if (cmd[0] == '%') {
        skip_cwd = true;
        lookup_name = cmd + 1;
        if (lookup_name[0] == '\0') return false;
    }

    if (!skip_cwd && strchr(lookup_name, '/') != NULL) {
        if (is_executable(lookup_name)) {
            strncpy(resolved_path, lookup_name, max_len - 1);
            resolved_path[max_len - 1] = '\0';
            return true;
        }
        return false;
    }

    if (!skip_cwd) {
        char cwd_candidate[MAX_PATH_BUF];
        safe_join_path(cwd_candidate, sizeof(cwd_candidate), cwd, lookup_name);
        if (is_executable(cwd_candidate)) {
            strncpy(resolved_path, cwd_candidate, max_len - 1);
            resolved_path[max_len - 1] = '\0';
            return true;
        }
    }

    const char *path_env = getenv("PATH");
    if (path_env && path_env[0] != '\0') {
        char *path_copy = strdup(path_env);
        if (path_copy) {
            char *saveptr;
            char *dir = strtok_r(path_copy, ":", &saveptr);

            while (dir != NULL) {
                char candidate[MAX_PATH_BUF];
                if (dir[0] == '\0') {
                    if (!skip_cwd) {
                        safe_join_path(candidate, sizeof(candidate), cwd, lookup_name);
                    } else {
                        dir = strtok_r(NULL, ":", &saveptr);
                        continue;
                    }
                } else {
                    safe_join_path(candidate, sizeof(candidate), dir, lookup_name);
                }

                if (is_executable(candidate)) {
                    strncpy(resolved_path, candidate, max_len - 1);
                    resolved_path[max_len - 1] = '\0';
                    free(path_copy);
                    return true;
                }
                dir = strtok_r(NULL, ":", &saveptr);
            }
            free(path_copy);
        }
    }

    return false;
}

static bool is_builtin(const char *name) {
    return (strcmp(name, "hop") == 0 ||
            strcmp(name, "reveal") == 0 ||
            strcmp(name, "peek") == 0 ||
            strcmp(name, "locate") == 0 ||
            strcmp(name, "activities") == 0 ||
            strcmp(name, "resume") == 0 ||
            strcmp(name, "ping") == 0 ||
            strcmp(name, "spy") == 0 ||
            strcmp(name, "snoop") == 0);
}

static int dispatch_builtin(int argc, char **argv, const char *shell_home) {
    if (strcmp(argv[0], "hop") == 0) return builtin_hop(argc, argv, shell_home);
    if (strcmp(argv[0], "reveal") == 0) return builtin_reveal(argc, argv, shell_home);
    if (strcmp(argv[0], "peek") == 0) return builtin_peek(argc, argv);
    if (strcmp(argv[0], "locate") == 0) return builtin_locate(argc, argv);
    if (strcmp(argv[0], "activities") == 0) return builtin_activities(argc, argv);
    if (strcmp(argv[0], "resume") == 0) return builtin_resume(argc, argv);
    if (strcmp(argv[0], "ping") == 0) return builtin_ping(argc, argv);
    if (strcmp(argv[0], "spy") == 0) return builtin_spy(argc, argv);
    if (strcmp(argv[0], "snoop") == 0) return builtin_snoop(argc, argv);
    return -1;
}

static int setup_input_stream(char **files, size_t count, pid_t *feeder_pid) {
    if (count == 0) {
        *feeder_pid = -1;
        return -1;
    }

    if (count == 1) {
        *feeder_pid = -1;
        return open(files[0], O_RDONLY);
    }

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe failed");
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork feeder failed");
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (pid == 0) {
        close(pipefd[0]);
        char buf[STREAM_CHUNK_SIZE];

        for (size_t i = 0; i < count; i++) {
            int fd = open(files[i], O_RDONLY);
            if (fd >= 0) {
                ssize_t bytes;
                while ((bytes = read(fd, buf, sizeof(buf))) > 0) {
                    ssize_t written = 0;
                    while (written < bytes) {
                        ssize_t n = write(pipefd[1], buf + written, bytes - written);
                        if (n <= 0) break;
                        written += n;
                    }
                }
                close(fd);
            }
        }

        close(pipefd[1]);
        _exit(0);
    }

    close(pipefd[1]);
    *feeder_pid = pid;
    return pipefd[0];
}

pid_t execute_single_command(Command *cmd, const char *shell_home, int pipe_in, int pipe_out, bool in_pipeline, pid_t pgid, bool is_bg) {
    if (cmd->argc == 0) return 0;

    bool is_bltin = false;
    if (cmd->argv[0][0] != '%' && is_builtin(cmd->argv[0])) {
        is_bltin = true;
    }

    if (is_bltin && !in_pipeline && !is_bg) {
        int saved_stdin = -1;
        int saved_stdout = -1;
        pid_t feeder_pid = -1;
        pid_t tee_pid = -1;

        if (cmd->input_count > 0) {
            int in_fd = setup_input_stream(cmd->input_files, cmd->input_count, &feeder_pid);
            if (in_fd >= 0) {
                saved_stdin = dup(STDIN_FILENO);
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            }
        }

        if (cmd->output_count > 0) {
            int pipefd[2];
            pipe(pipefd);
            tee_pid = fork();
            if (tee_pid == 0) {
                close(pipefd[1]);
                int fds[cmd->output_count];
                for (int i = 0; i < cmd->output_count; i++) {
                    int flags = O_WRONLY | O_CREAT | (cmd->output_modes[i] ? O_APPEND : O_TRUNC);
                    fds[i] = open(cmd->output_files[i], flags, 0644);
                }
                char buf[4096];
                ssize_t n;
                while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
                    for (int i = 0; i < cmd->output_count; i++)
                        write(fds[i], buf, n);
                }
                for (int i = 0; i < cmd->output_count; i++) close(fds[i]);
                close(pipefd[0]);
                _exit(0);
            }
            close(pipefd[0]);
            saved_stdout = dup(STDOUT_FILENO);
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);
        }

        dispatch_builtin(cmd->argc, cmd->argv, shell_home);

        if (saved_stdin != -1) {
            dup2(saved_stdin, STDIN_FILENO);
            close(saved_stdin);
        }
        if (saved_stdout != -1) {
            dup2(saved_stdout, STDOUT_FILENO);
            close(saved_stdout);
        }
        if (feeder_pid > 0) waitpid(feeder_pid, NULL, 0);
        if (tee_pid > 0) waitpid(tee_pid, NULL, 0);

        return 0;
    }

    char resolved_path[MAX_PATH_BUF];
    if (!is_bltin) {
        if (!resolve_executable_path(cmd->argv[0], resolved_path, sizeof(resolved_path))) {
            const char *p = cmd->argv[0]; if(p[0]=='%') p++; printf("cshell: command not found (%s)\n", p);
            return -1;
        }
        if (cmd->argv[0][0] == '%') cmd->argv[0]++;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        return -1;
    }

    if (pid == 0) {
        pid_t child_pgid = (pgid == 0) ? getpid() : pgid;
        setpgid(0, child_pgid);

        if (pipe_in != -1) {
            dup2(pipe_in, STDIN_FILENO);
            close(pipe_in);
        }
        if (pipe_out != -1) {
            dup2(pipe_out, STDOUT_FILENO);
            close(pipe_out);
        }

        if (cmd->input_count > 0) {
            pid_t feeder_pid = -1;
            int in_fd = setup_input_stream(cmd->input_files, cmd->input_count, &feeder_pid);
            if (in_fd >= 0) {
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            } else {
                exit(EXIT_FAILURE);
            }
        } else if (is_bg && pipe_in == -1) {
            int devnull = open("/dev/null", O_RDONLY);
            if (devnull != -1) {
                dup2(devnull, STDIN_FILENO);
                close(devnull);
            }
        }

        if (cmd->output_count > 0) {
            int pipefd[2];
            pipe(pipefd);
            pid_t tee = fork();
            if (tee == 0) {
                close(pipefd[1]);
                int fds[cmd->output_count];
                for (int i = 0; i < cmd->output_count; i++) {
                    int flags = O_WRONLY | O_CREAT | (cmd->output_modes[i] ? O_APPEND : O_TRUNC);
                    fds[i] = open(cmd->output_files[i], flags, 0644);
                }
                char buf[4096];
                ssize_t n;
                while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
                    for (int i = 0; i < cmd->output_count; i++)
                        write(fds[i], buf, n);
                }
                for (int i = 0; i < cmd->output_count; i++) close(fds[i]);
                close(pipefd[0]);
                _exit(0);
            }
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);
        }

        if (is_bltin) {
            dispatch_builtin(cmd->argc, cmd->argv, shell_home);
            exit(0);
        } else {
            execv(resolved_path, cmd->argv);
            perror("execv failed");
            exit(EXIT_FAILURE);
        }
    }
    
    pid_t child_pgid = (pgid == 0) ? pid : pgid;
    setpgid(pid, child_pgid);

    return pid;
}