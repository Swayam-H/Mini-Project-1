#ifndef PROMPT_H
#define PROMPT_H

#include <stddef.h>

void format_path(const char *path, const char *shell_home, char *formatted_path, size_t size);

void display_prompt(const char *shell_home);

#endif 