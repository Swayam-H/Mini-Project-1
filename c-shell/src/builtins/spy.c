#include "builtins.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdbool.h>
#include <limits.h>

static const char *get_file_type(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return "unknown";

    if (S_ISREG(st.st_mode))  return "REG";
    if (S_ISDIR(st.st_mode))  return "DIR";
    if (S_ISCHR(st.st_mode))  return "CHR";
    if (S_ISBLK(st.st_mode))  return "BLK";
    if (S_ISFIFO(st.st_mode)) return "FIFO";
    if (S_ISLNK(st.st_mode))  return "LINK";
    if (S_ISSOCK(st.st_mode)) return "SOCK";
    return "unknown";
}

int builtin_spy(int argc, char **argv) {
    if (argc > 2) { printf("spy: invalid syntax\n"); return -1; }

    pid_t pid;
    if (argc == 2) {
        pid = atoi(argv[1]);
    } else {
        pid = getpid();
    }

    char proc_dir[256];
    snprintf(proc_dir, sizeof(proc_dir), "/proc/%d", pid);
    if (access(proc_dir, F_OK) != 0) {
        printf("spy: no such process\n");
        return -1;
    }

    printf("PID\tFD\tTYPE\tPATH\n");

    char link_path[512], target[PATH_MAX];
    ssize_t n;

    snprintf(link_path, sizeof(link_path), "/proc/%d/cwd", pid);
    n = readlink(link_path, target, sizeof(target) - 1);
    if (n > 0) {
        target[n] = '\0';
        printf("%d\tcwd\t%s\t%s\n", pid, get_file_type(target), target);
    }

    snprintf(link_path, sizeof(link_path), "/proc/%d/exe", pid);
    n = readlink(link_path, target, sizeof(target) - 1);
    if (n > 0) {
        target[n] = '\0';
        printf("%d\ttxt\t%s\t%s\n", pid, get_file_type(target), target);
    }

    snprintf(link_path, sizeof(link_path), "/proc/%d/maps", pid);
    FILE *maps = fopen(link_path, "r");
    if (maps) {
        char *seen[1024]; int seen_count = 0;
        char map_line[4096];
        while (fgets(map_line, sizeof(map_line), maps)) {
            char *p = map_line;
            int fields = 0;
            while (*p && fields < 5) {
                while (*p && *p != ' ' && *p != '\t') p++;
                while (*p == ' ' || *p == '\t') p++;
                fields++;
            }
            char *nl = strchr(p, '\n');
            if (nl) *nl = '\0';

            if (*p == '/') {
                bool dup = false;
                for (int i = 0; i < seen_count; i++) {
                    if (strcmp(seen[i], p) == 0) { dup = true; break; }
                }
                if (!dup && seen_count < 1024) {
                    seen[seen_count] = strdup(p);
                    printf("%d\tmem\t%s\t%s\n", pid, get_file_type(p), p);
                    seen_count++;
                }
            }
        }
        fclose(maps);
        for (int i = 0; i < seen_count; i++) free(seen[i]);
    }

    char fd_dir[256];
    snprintf(fd_dir, sizeof(fd_dir), "/proc/%d/fd", pid);
    DIR *dir = opendir(fd_dir);
    if (dir) {
        struct dirent *ent;
        int fd_nums[1024]; int fd_count = 0;
        while ((ent = readdir(dir)) != NULL && fd_count < 1024) {
            if (ent->d_name[0] == '.') continue;
            fd_nums[fd_count++] = atoi(ent->d_name);
        }
        closedir(dir);

        for (int i = 0; i < fd_count - 1; i++)
            for (int j = i + 1; j < fd_count; j++)
                if (fd_nums[i] > fd_nums[j]) {
                    int tmp = fd_nums[i]; fd_nums[i] = fd_nums[j]; fd_nums[j] = tmp;
                }

        for (int i = 0; i < fd_count; i++) {
            snprintf(link_path, sizeof(link_path), "%s/%d", fd_dir, fd_nums[i]);
            n = readlink(link_path, target, sizeof(target) - 1);
            if (n > 0) {
                target[n] = '\0';
                printf("%d\t%d\t%s\t%s\n", pid, fd_nums[i], get_file_type(target), target);
            }
        }
    }

    return 0;
}
