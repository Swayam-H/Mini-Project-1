#include "prompt.h"
#include "lexer.h"
#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

int main(void) {
    char shell_home[PATH_MAX];

    if (getcwd(shell_home, sizeof(shell_home)) == NULL) {
        perror("getcwd failed");
        return 1;
    }

    char *line = NULL;
    size_t len = 0;
    ssize_t nread;

    while (1) {
        display_prompt(shell_home);

        nread = getline(&line, &len, stdin);
        if (nread == -1) {
            printf("\n");
            break;
        }

        if (nread > 0 && line[nread - 1] == '\n') line[nread - 1] = '\0';
        if (nread > 0 && line[nread - 1] == '\r') line[nread - 1] = '\0';

        TokenStream *tokens = tokenize(line);

        if (!tokens || !validate_syntax(tokens)) {
            printf("cshell: invalid syntax\n");
            fflush(stdout);
        } else {
            
        }

        free_token_stream(tokens);
    }

    free(line);
    return 0;
}