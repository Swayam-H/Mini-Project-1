#include "builtins.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdbool.h>

#define FRECENCY_FILE ".cshell_frecency"
#define MAX_ENTRIES 512

typedef struct {
    char path[PATH_MAX];
    double count;
    time_t last_visited;
} FrecencyEntry;

static char prev_cwd[PATH_MAX] = "";
static bool has_prev_cwd = false;

bool get_prev_cwd(char *dest, size_t size) {
    if (!has_prev_cwd || !dest || size == 0) {
        return false;
    }
    strncpy(dest, prev_cwd, size - 1);
    dest[size - 1] = '\0';
    return true;
}

void set_prev_cwd(const char *path) {
    if (!path) return;
    strncpy(prev_cwd, path, sizeof(prev_cwd) - 1);
    prev_cwd[sizeof(prev_cwd) - 1] = '\0';
    has_prev_cwd = true;
}

static void get_frecency_filepath(char *dest, size_t size) {
    const char *home = getenv("HOME");
    if (!home) {
        home = ".";
    }
    snprintf(dest, size, "%s/%s", home, FRECENCY_FILE);
}

static int load_frecency(FrecencyEntry entries[], int max_count) {
    char filepath[PATH_MAX];
    get_frecency_filepath(filepath, sizeof(filepath));

    FILE *fp = fopen(filepath, "r");
    if (!fp) return 0;

    int count = 0;
    char line[PATH_MAX + 64];
    while (count < max_count && fgets(line, sizeof(line), fp)) {
        char p[PATH_MAX];
        double c;
        long long t;
        if (sscanf(line, "%lf %lld %[^\n]", &c, &t, p) == 3) {
            strncpy(entries[count].path, p, PATH_MAX - 1);
            entries[count].path[PATH_MAX - 1] = '\0';
            entries[count].count = c;
            entries[count].last_visited = (time_t)t;
            count++;
        }
    }
    fclose(fp);
    return count;
}

static void save_frecency(const FrecencyEntry entries[], int count) {
    char filepath[PATH_MAX];
    get_frecency_filepath(filepath, sizeof(filepath));

    FILE *fp = fopen(filepath, "w");
    if (!fp) return;

    for (int i = 0; i < count; i++) {
        fprintf(fp, "%.4f %lld %s\n", 
                entries[i].count, 
                (long long)entries[i].last_visited,
                entries[i].path);
    }
    fclose(fp);
}

static double calculate_score(const FrecencyEntry *entry, time_t now) {
    double delta_seconds = difftime(now, entry->last_visited);
    if (delta_seconds < 0) delta_seconds = 0;

    double delta_hours = delta_seconds / 3600.0;
    return entry->count / (1.0 + 0.25 * delta_hours);
}

static void record_hop_success(const char *resolved_path) {
    FrecencyEntry entries[MAX_ENTRIES];
    int count = load_frecency(entries, MAX_ENTRIES);
    time_t now = time(NULL);

    int idx = -1;
    for (int i = 0; i < count; i++) {
        if (strcmp(entries[i].path, resolved_path) == 0) {
            idx = i;
            break;
        }
    }

    if (idx != -1) {
        entries[idx].count += 1.0;
        entries[idx].last_visited = now;
    } else if (count < MAX_ENTRIES) {
        strncpy(entries[count].path, resolved_path, PATH_MAX - 1);
        entries[count].path[PATH_MAX - 1] = '\0';
        entries[count].count = 1.0;
        entries[count].last_visited = now;
        count++;
    }

    save_frecency(entries, count);
}

static bool frecency_hop(const char *name) {
    FrecencyEntry entries[MAX_ENTRIES];
    int count = load_frecency(entries, MAX_ENTRIES);
    if (count == 0) return false;

    time_t now = time(NULL);

    int best_index = -1;
    double best_score = -1.0;

    for (int i = 0; i < count; i++) {
        if (strstr(entries[i].path, name) != NULL) {
            struct stat st;
            if (stat(entries[i].path, &st) == 0 && S_ISDIR(st.st_mode)) {
                double score = calculate_score(&entries[i], now);
                if (score > best_score) {
                    best_score = score;
                    best_index = i;
                }
            }
        }
    }

    if (best_index != -1) {
        if (chdir(entries[best_index].path) == 0) {
            record_hop_success(entries[best_index].path);
            return true;
        }
    }

    return false;
}

static int execute_single_hop(const char *arg, const char *shell_home) {
    char current_cwd[PATH_MAX];
    if (getcwd(current_cwd, sizeof(current_cwd)) == NULL) {
        perror("getcwd");
        return -1;
    }

    char target[PATH_MAX];

    if (strcmp(arg, "~") == 0) {
        snprintf(target, sizeof(target), "%s", shell_home);
    }
    else if (strcmp(arg, ".") == 0) {
        return 0; 
    }
    else if (strcmp(arg, "..") == 0) {
        strncpy(target, "..", sizeof(target));
    }
    else if (strcmp(arg, "-") == 0) {
        if (!has_prev_cwd) {
            return 0; 
        }
        strncpy(target, prev_cwd, sizeof(target));
    }
    else {
        if (arg[0] == '~' && (arg[1] == '/' || arg[1] == '\0')) {
            snprintf(target, sizeof(target), "%s%s", shell_home, arg + 1);
        } else {
            strncpy(target, arg, sizeof(target) - 1);
            target[sizeof(target) - 1] = '\0';
        }
    }

    if (chdir(target) == 0) {
        strncpy(prev_cwd, current_cwd, sizeof(prev_cwd));
        has_prev_cwd = true;

        char resolved[PATH_MAX];
        if (getcwd(resolved, sizeof(resolved)) != NULL) {
            record_hop_success(resolved);
        }
        return 0;
    }

    if (strcmp(arg, "~") != 0 && strcmp(arg, "..") != 0 && strcmp(arg, "-") != 0) {
        if (frecency_hop(arg)) {
            strncpy(prev_cwd, current_cwd, sizeof(prev_cwd));
            has_prev_cwd = true;
            return 0;
        }
    }

    printf("hop: no such directory\n");
    return -1;
}

int builtin_hop(int argc, char **argv, const char *shell_home) {
   
    if (argc <= 1) {
        return execute_single_hop("~", shell_home);
    }

    for (int i = 1; i < argc; i++) {
        if (execute_single_hop(argv[i], shell_home) != 0) {
            return -1;
        }
    }

    return 0;
}