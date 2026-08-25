#ifndef BUILTINS_H
#define BUILTINS_H

#include <stddef.h>
#include <stdbool.h>

int builtin_hop(int argc, char **argv, const char *shell_home);

int builtin_reveal(int argc, char **argv, const char *shell_home);

void set_prev_cwd(const char *path);

bool get_prev_cwd(char *dest, size_t size);

int builtin_peek(int argc, char **argv);

int builtin_locate(int argc, char **argv);

#endif 