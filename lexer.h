#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

typedef enum {
    WORD,       
    OP_PIPE,       
    OP_AMP,        
    OP_SEMI,       
    OP_LT,         
    OP_GT,         
    OP_GTGT,       
    EOF,        
    ERROR      
} TokenType;

typedef struct {
    TokenType type;
    char *value;    
} Token;

typedef struct {
    Token *tokens;
    size_t count;
    size_t capacity;
} TokenStream;

TokenStream *tokenize(const char *input);

void free_token_stream(TokenStream *stream);

#endif 