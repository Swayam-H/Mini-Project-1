#include "builtins.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <stdbool.h>

static void build_path(char *dest, size_t size, const char *dir, const char *name) {
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

static bool is_executable_file(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }
    if (!S_ISREG(st.st_mode)) {
        return false;
    }
    return (access(path, X_OK) == 0);
}

static void locate_single_command(const char *name) {
    bool found_any = false;

    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        char cwd_candidate[PATH_MAX * 2];
        build_path(cwd_candidate, sizeof(cwd_candidate), cwd, name);

        if (is_executable_file(cwd_candidate)) {
            printf("%s\n", cwd_candidate);
            found_any = true;
        }
    }

    const char *path_env = getenv("PATH");
    if (path_env && path_env[0] != '\0') {
        char *path_copy = strdup(path_env);
        if (path_copy) {
            char *saveptr;
            char *dir = strtok_r(path_copy, ":", &saveptr);

            while (dir != NULL) {
                char full_path[PATH_MAX * 2];

                if (dir[0] == '\0') {
                    build_path(full_path, sizeof(full_path), cwd, name);
                } else {
                    build_path(full_path, sizeof(full_path), dir, name);
                }

                if (is_executable_file(full_path)) {
                    printf("%s\n", full_path);
                    found_any = true;
                }

                dir = strtok_r(NULL, ":", &saveptr);
            }

            free(path_copy);
        }
    }

    if (!found_any) {
        printf("locate: command not found (%s)\n", name);
    }
}

int builtin_locate(int argc, char **argv) {
    if (argc <= 1) {
        return 0;
    }

    for (int i = 1; i < argc; i++) {
        locate_single_command(argv[i]);
    }

    return 0;
}