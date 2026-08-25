#include "builtins.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdbool.h>

#define CHUNK_SIZE 4096

static void output_line(const char *line, bool flag_n, size_t *line_counter) {
    if (flag_n) {
        if (line[0] != '\0') {
            (*line_counter)++;
            printf("%zu %s\n", *line_counter, line);
        } else {
            printf("\n");
        }
    } else {
        printf("%s\n", line);
    }
}

static void peek_forward_stream(int fd, bool flag_n) {
    size_t line_counter = 0;
    char buffer[CHUNK_SIZE];
    ssize_t bytes_read;

    size_t line_cap = 256;
    size_t line_len = 0;
    char *current_line = malloc(line_cap);

    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        for (ssize_t i = 0; i < bytes_read; i++) {
            char c = buffer[i];
            if (c == '\n') {
                current_line[line_len] = '\0';
                output_line(current_line, flag_n, &line_counter);
                line_len = 0;
            } else {
                if (line_len + 1 >= line_cap) {
                    line_cap *= 2;
                    current_line = realloc(current_line, line_cap);
                }
                current_line[line_len++] = c;
            }
        }
    }

    if (line_len > 0) {
        current_line[line_len] = '\0';
        output_line(current_line, flag_n, &line_counter);
    }

    free(current_line);
}

static void peek_reverse_nonseekable(int fd, bool flag_n) {
    size_t lines_cap = 64;
    size_t lines_count = 0;
    char **lines = malloc(lines_cap * sizeof(char *));

    size_t line_cap = 256;
    size_t line_len = 0;
    char *current_line = malloc(line_cap);

    char buffer[CHUNK_SIZE];
    ssize_t bytes_read;

    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        for (ssize_t i = 0; i < bytes_read; i++) {
            char c = buffer[i];
            if (c == '\n') {
                current_line[line_len] = '\0';
                if (lines_count >= lines_cap) {
                    lines_cap *= 2;
                    lines = realloc(lines, lines_cap * sizeof(char *));
                }
                lines[lines_count++] = strdup(current_line);
                line_len = 0;
            } else {
                if (line_len + 1 >= line_cap) {
                    line_cap *= 2;
                    current_line = realloc(current_line, line_cap);
                }
                current_line[line_len++] = c;
            }
        }
    }

    if (line_len > 0) {
        current_line[line_len] = '\0';
        if (lines_count >= lines_cap) {
            lines_cap *= 2;
            lines = realloc(lines, lines_cap * sizeof(char *));
        }
        lines[lines_count++] = strdup(current_line);
    }
    free(current_line);

    size_t *line_nums = NULL;
    if (flag_n) {
        line_nums = malloc(lines_count * sizeof(size_t));
        size_t counter = 0;
        for (size_t i = 0; i < lines_count; i++) {
            if (lines[i][0] != '\0') {
                counter++;
                line_nums[i] = counter;
            } else {
                line_nums[i] = 0;
            }
        }
    }

    for (ssize_t i = (ssize_t)lines_count - 1; i >= 0; i--) {
        if (flag_n) {
            if (line_nums[i] > 0) {
                printf("%zu %s\n", line_nums[i], lines[i]);
            } else {
                printf("\n");
            }
        } else {
            printf("%s\n", lines[i]);
        }
        free(lines[i]);
    }

    free(line_nums);
    free(lines);
}

static void peek_reverse_seekable(int fd, off_t file_size, bool flag_n) {
    if (file_size == 0) return;

    size_t *line_nums = NULL;
    size_t total_lines = 0;
    if (flag_n) {
        lseek(fd, 0, SEEK_SET);
        size_t cap = 128;
        line_nums = malloc(cap * sizeof(size_t));
        
        char buf[CHUNK_SIZE];
        ssize_t n;
        bool in_line = false;
        bool non_empty = false;
        size_t counter = 0;

        while ((n = read(fd, buf, sizeof(buf))) > 0) {
            for (ssize_t i = 0; i < n; i++) {
                if (buf[i] == '\n') {
                    if (total_lines >= cap) {
                        cap *= 2;
                        line_nums = realloc(line_nums, cap * sizeof(size_t));
                    }
                    if (non_empty) {
                        counter++;
                        line_nums[total_lines++] = counter;
                    } else {
                        line_nums[total_lines++] = 0;
                    }
                    non_empty = false;
                    in_line = false;
                } else {
                    in_line = true;
                    non_empty = true;
                }
            }
        }
        if (in_line) {
            if (total_lines >= cap) {
                cap *= 2;
                line_nums = realloc(line_nums, cap * sizeof(size_t));
            }
            if (non_empty) {
                counter++;
                line_nums[total_lines++] = counter;
            } else {
                line_nums[total_lines++] = 0;
            }
        }
    }

    off_t offset = file_size;
    char chunk[CHUNK_SIZE];
    
    size_t remainder_cap = 256;
    size_t remainder_len = 0;
    char *remainder = malloc(remainder_cap);

    ssize_t current_line_idx = (ssize_t)total_lines - 1;

    while (offset > 0) {
        size_t bytes_to_read = (offset < CHUNK_SIZE) ? (size_t)offset : CHUNK_SIZE;
        offset -= bytes_to_read;
        lseek(fd, offset, SEEK_SET);

        ssize_t bytes_read = read(fd, chunk, bytes_to_read);
        if (bytes_read <= 0) break;

        for (ssize_t i = bytes_read - 1; i >= 0; i--) {
            char c = chunk[i];
            if (c == '\n') {
                if ((off_t)(offset + i) == file_size - 1) {
                    continue;
                }

                char *line = malloc(remainder_len + 1);
                for (size_t k = 0; k < remainder_len; k++) {
                    line[k] = remainder[remainder_len - 1 - k];
                }
                line[remainder_len] = '\0';

                if (flag_n && line_nums && current_line_idx >= 0) {
                    if (line_nums[current_line_idx] > 0) {
                        printf("%zu %s\n", line_nums[current_line_idx], line);
                    } else {
                        printf("\n");
                    }
                    current_line_idx--;
                } else {
                    printf("%s\n", line);
                }

                free(line);
                remainder_len = 0;
            } else {
                if (remainder_len + 1 >= remainder_cap) {
                    remainder_cap *= 2;
                    remainder = realloc(remainder, remainder_cap);
                }
                remainder[remainder_len++] = c;
            }
        }
    }

    if (remainder_len > 0) {
        char *line = malloc(remainder_len + 1);
        for (size_t k = 0; k < remainder_len; k++) {
            line[k] = remainder[remainder_len - 1 - k];
        }
        line[remainder_len] = '\0';

        if (flag_n && line_nums && current_line_idx >= 0) {
            if (line_nums[current_line_idx] > 0) {
                printf("%zu %s\n", line_nums[current_line_idx], line);
            } else {
                printf("\n");
            }
        } else {
            printf("%s\n", line);
        }

        free(line);
    }

    free(remainder);
    if (line_nums) free(line_nums);
}

static void process_peek_source(const char *filename, bool flag_n, bool flag_r) {
    if (!filename || strcmp(filename, "-") == 0) {
        if (flag_r) {
            peek_reverse_nonseekable(STDIN_FILENO, flag_n);
        } else {
            peek_forward_stream(STDIN_FILENO, flag_n);
        }
        return;
    }

    struct stat st;
    if (stat(filename, &st) != 0) {
        printf("peek: no such file or directory\n");
        return;
    }

    if (S_ISDIR(st.st_mode)) {
        printf("peek: is a directory\n");
        return;
    }

    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        printf("peek: no such file or directory\n");
        return;
    }

    if (flag_r) {
        if (S_ISREG(st.st_mode)) {
            peek_reverse_seekable(fd, st.st_size, flag_n);
        } else {
            peek_reverse_nonseekable(fd, flag_n);
        }
    } else {
        peek_forward_stream(fd, flag_n);
    }

    close(fd);
}

int builtin_peek(int argc, char **argv) {
    bool flag_n = false;
    bool flag_r = false;

    size_t files_cap = 8;
    size_t files_count = 0;
    char **files = malloc(files_cap * sizeof(char *));

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (arg[0] == '-' && arg[1] != '\0') {
            for (size_t j = 1; arg[j] != '\0'; j++) {
                if (arg[j] == 'n') {
                    flag_n = true;
                } else if (arg[j] == 'r') {
                    flag_r = true;
                } else {
                    printf("peek: invalid syntax\n");
                    free(files);
                    return -1;
                }
            }
        } else {
            if (files_count >= files_cap) {
                files_cap *= 2;
                files = realloc(files, files_cap * sizeof(char *));
            }
            files[files_count++] = (char *)arg;
        }
    }

    if (files_count == 0) {
        process_peek_source(NULL, flag_n, flag_r);
    } else {
        for (size_t i = 0; i < files_count; i++) {
            process_peek_source(files[i], flag_n, flag_r);
        }
    }

    free(files);
    return 0;
}