#include "builtins.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include <stdbool.h>

static int compare_entries(const void *a, const void *b) {
    const char *str_a = *(const char **)a;
    const char *str_b = *(const char **)b;
    return strcmp(str_a, str_b);
}

static void list_directory(const char *dir_path, const char *display_prefix, bool show_all, bool recursive) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        return;
    }

    size_t count = 0;
    size_t cap = 32;
    char **entries = malloc(cap * sizeof(char *));
    if (!entries) {
        closedir(dir);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
  
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (!show_all && entry->d_name[0] == '.') {
            continue;
        }

        if (count >= cap) {
            cap *= 2;
            entries = realloc(entries, cap * sizeof(char *));
        }
        entries[count++] = strdup(entry->d_name);
    }
    closedir(dir);

    qsort(entries, count, sizeof(char *), compare_entries);

    for (size_t i = 0; i < count; i++) {
        char full_entry_path[PATH_MAX];
        snprintf(full_entry_path, sizeof(full_entry_path), "%s/%s", dir_path, entries[i]);

        struct stat st;
        bool is_dir = false;
        if (stat(full_entry_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            is_dir = true;
        }

        if (recursive) {
            if (is_dir) {
                printf("%s%s/\n", display_prefix, entries[i]);
                char next_prefix[PATH_MAX];
                snprintf(next_prefix, sizeof(next_prefix), "%s%s/", display_prefix, entries[i]);
                list_directory(full_entry_path, next_prefix, show_all, recursive);
            } else {
                printf("%s%s\n", display_prefix, entries[i]);
            }
        } else {
            printf("%s\n", entries[i]);
        }

        free(entries[i]);
    }

    free(entries);
}

int builtin_reveal(int argc, char **argv, const char *shell_home) {
    bool flag_a = false;
    bool flag_t = false;
    const char *target_arg = NULL;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (arg[0] == '-' && arg[1] != '\0') {
            if (target_arg != NULL) {
                printf("reveal: invalid syntax\n");
                return -1;
            }
            for (size_t j = 1; arg[j] != '\0'; j++) {
                if (arg[j] == 'a') {
                    flag_a = true;
                } else if (arg[j] == 't') {
                    flag_t = true;
                } else {
                    printf("reveal: invalid syntax\n");
                    return -1;
                }
            }
        } else {
            if (target_arg != NULL) {
                printf("reveal: invalid syntax\n");
                return -1;
            }
            target_arg = arg;
        }
    }

    char resolved_path[PATH_MAX];

    if (!target_arg || strcmp(target_arg, ".") == 0) {
        if (getcwd(resolved_path, sizeof(resolved_path)) == NULL) {
            printf("reveal: no such directory\n");
            return -1;
        }
    } else if (strcmp(target_arg, "~") == 0) {
        strncpy(resolved_path, shell_home, sizeof(resolved_path));
    } else if (strcmp(target_arg, "..") == 0) {
        strncpy(resolved_path, "..", sizeof(resolved_path));
    } else if (strcmp(target_arg, "-") == 0) {
        if (!get_prev_cwd(resolved_path, sizeof(resolved_path))) {
            printf("reveal: no such directory\n");
            return -1;
        }
    } else {
        if (target_arg[0] == '~' && (target_arg[1] == '/' || target_arg[1] == '\0')) {
            snprintf(resolved_path, sizeof(resolved_path), "%s%s", shell_home, target_arg + 1);
        } else {
            strncpy(resolved_path, target_arg, sizeof(resolved_path) - 1);
            resolved_path[sizeof(resolved_path) - 1] = '\0';
        }
    }

    struct stat st;
    if (stat(resolved_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        printf("reveal: no such directory\n");
        return -1;
    }

    list_directory(resolved_path, "", flag_a, flag_t);
    return 0;
}