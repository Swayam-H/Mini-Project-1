#ifndef EXECUTE_H
#define EXECUTE_H

#include "lexer.h"
#include <stddef.h>
#include <stdbool.h>

#include <sys/types.h>

typedef struct {
    char **argv;
    int argc;
    char **input_files;
    size_t input_count;
    char **output_files;  
    int *output_modes;        
    int output_count; 
} Command;

pid_t execute_single_command(Command *cmd, const char *shell_home, int pipe_in, int pipe_out, bool in_pipeline);

#endif 