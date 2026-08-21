#include "prompt.h"
#include "lexer.h"
#include "parser.h"
#include "builtins.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

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
        } else if (tokens->count > 1) { 
            
            char **argv = malloc((tokens->count) * sizeof(char *));
            int argc = 0;

            for (size_t i = 0; i < tokens->count && tokens->tokens[i].type == WORD; i++) {
                argv[argc++] = tokens->tokens[i].value;
            }
            argv[argc] = NULL;

            if (argc > 0 && strcmp(argv[0], "hop") == 0) {
                builtin_hop(argc, argv, shell_home);
            }else if (strcmp(argv[0], "reveal") == 0) {
                builtin_reveal(argc, argv, shell_home);
            }else if (strcmp(argv[0], "peek") == 0) {
                builtin_peek(argc, argv);
            }

            free(argv);
        }

        free_token_stream(tokens);
    }

    free(line);
    return 0;
}