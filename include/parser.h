#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include <stdbool.h>

bool validate_syntax(const TokenStream *stream);

#endif 