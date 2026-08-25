#include "prompt.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>


void format_path(const char *path,const char *shell_home,char *formatted_path, size_t size) {
    if (!path || !shell_home || !formatted_path || size == 0) {
        return;
    }

    size_t shell_home_len = strlen(shell_home);

    if (strcmp(path, shell_home) == 0) {
        snprintf(formatted_path, size, "~");
        return;
    }

    if (strncmp(path, shell_home, shell_home_len) == 0 && path[shell_home_len] == '/') {
        snprintf(formatted_path, size, "~%s", path + shell_home_len);
        return;
    }

    snprintf(formatted_path, size, "%s", path);

}

void display_prompt(const char *shell_home) {
    char hostname[HOST_NAME_MAX];
    char cwd[PATH_MAX];
    char prompt_path[PATH_MAX];

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd failed");
        return;
    }
    
    struct passwd *pw = getpwuid(getuid());
    const char *username = pw ? pw->pw_name : "unknown";

    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strncpy(hostname, "unknown", sizeof(hostname));
    }

    format_path(cwd, shell_home, prompt_path, sizeof(prompt_path));

    printf("<%s@%s:%s> ", username, hostname, prompt_path);
    fflush(stdout);
}
