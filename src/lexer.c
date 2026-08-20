#include "lexer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static void token_stream_init(TokenStream *stream) {
    stream->capacity = 16;
    stream->count = 0;
    stream->tokens = malloc(stream->capacity * sizeof(Token));
}

static void token_stream_push(TokenStream *stream, TokenType type, char *value) {
    if (stream->count >= stream->capacity) {
        stream->capacity *= 2;
        stream->tokens = realloc(stream->tokens, stream->capacity * sizeof(Token));
    }
    stream->tokens[stream->count].type = type;
    stream->tokens[stream->count].value = value;
    stream->count++;
}

void free_token_stream(TokenStream *stream) {
    if (!stream) return;
    for (size_t i = 0; i < stream->count; i++) {
        if (stream->tokens[i].value) {
            free(stream->tokens[i].value);
        }
    }
    free(stream->tokens);
    free(stream);
}

bool is_it_a_space(char c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}

bool is_it_special(char c) {
    return (c == '|' || c == '&' || c == ';' || c == '<' || c == '>');
}

TokenStream *tokenize(const char *input) {
    TokenStream *stream = malloc(sizeof(TokenStream));
    if (!stream) return NULL;
    token_stream_init(stream);

    size_t i = 0;
    size_t len = strlen(input);

    while (i < len) {
        
        if (is_it_a_space(input[i])) {
            i++;
            continue;
        }

        if (input[i] == '>') {
            if (i + 1 < len && input[i + 1] == '>') {
                token_stream_push(stream, OP_GTGT, NULL);
                i += 2;
            } else {
                token_stream_push(stream, OP_GT, NULL);
                i++;
            }
            continue;
        }

        if (input[i] == '<') {
            token_stream_push(stream, OP_LT, NULL);
            i++;
            continue;
        }
        if (input[i] == '|') {
            token_stream_push(stream, OP_PIPE, NULL);
            i++;
            continue;
        }
        if (input[i] == '&') {
            token_stream_push(stream, OP_AMP, NULL);
            i++;
            continue;
        }
        if (input[i] == ';') {
            token_stream_push(stream, OP_SEMI, NULL);
            i++;
            continue;
        }

        size_t word_cap = 32;
        size_t word_len = 0;
        char *word_buf = malloc(word_cap);

        bool in_word = true;
        bool lex_err = false;

        while (i < len && in_word) {
            char c = input[i];

            if (is_it_a_space(c) || is_it_special(c)) {
                break;
            }

            if (c == '\\') {
                i++;
                if (i >= len) {
                    lex_err = true; 
                    break;
                }
                if (word_len + 1 >= word_cap) {
                    word_cap *= 2;
                    word_buf = realloc(word_buf, word_cap);
                }
                word_buf[word_len++] = input[i++];
            } else if (c == '\'') {
                i++;
                bool closed = false;
                while (i < len) {
                    if (input[i] == '\'') {
                        closed = true;
                        i++;
                        break;
                    }
                    if (word_len + 1 >= word_cap) {
                        word_cap *= 2;
                        word_buf = realloc(word_buf, word_cap);
                    }
                    word_buf[word_len++] = input[i++];
                }
                if (!closed) {
                    lex_err = true;
                    break;
                }
            } else if (c == '"') {
                i++;
                bool closed = false;
                while (i < len) {
                    if (input[i] == '"') {
                        closed = true;
                        i++;
                        break;
                    }
                    if (input[i] == '\\') {
                        i++;
                        if (i >= len) {
                            lex_err = true;
                            break;
                        }
                        char next_c = input[i];
                        if (word_len + 2 >= word_cap) {
                            word_cap *= 2;
                            word_buf = realloc(word_buf, word_cap);
                        }
                        if (next_c == '"' || next_c == '\\') {
                            word_buf[word_len++] = next_c;
                        } else {
                            word_buf[word_len++] = '\\';
                            word_buf[word_len++] = next_c;
                        }
                        i++;
                    } else {
                        if (word_len + 1 >= word_cap) {
                            word_cap *= 2;
                            word_buf = realloc(word_buf, word_cap);
                        }
                        word_buf[word_len++] = input[i++];
                    }
                }
                if (!closed) {
                    lex_err = true;
                    break;
                }
            } else {
                if (word_len + 1 >= word_cap) {
                    word_cap *= 2;
                    word_buf = realloc(word_buf, word_cap);
                }
                word_buf[word_len++] = input[i++];
            }
        }

        if (lex_err) {
            free(word_buf);
            token_stream_push(stream, ERROR, NULL);
            return stream;
        }

        word_buf[word_len] = '\0';
        token_stream_push(stream, WORD, word_buf);
    }

    token_stream_push(stream, TOK_EOF, NULL);
    return stream;
}