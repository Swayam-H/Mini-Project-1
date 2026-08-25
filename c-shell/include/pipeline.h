#ifndef PIPELINE_H
#define PIPELINE_H

#include "lexer.h"

int execute_token_stream(const TokenStream *stream, const char *shell_home);

#endif
