#include "prompt.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

int main(void){
    char shell_home[PATH_MAX];

    if (getcwd(shell_home, sizeof(shell_home)) == NULL) {
        perror("getcwd failed");
        return 1;
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    while (1) {
        display_prompt(shell_home);

        read = getline(&line, &len, stdin);
        if (read == -1) {
            printf("\n");
            break; 
        }

        
        if (read > 0 && line[read - 1] == '\n') {
            line[read - 1] = '\0';
            read--;
        }

        if(read > 0 && line[read-1] == '\r'){
            line[read - 1] = '\0';
            read--;
        }

    }
    free(line);
    return 0;
}