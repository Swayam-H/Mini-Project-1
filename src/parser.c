#include "parser.h"
#include <stdio.h>

typedef struct {
    const TokenStream *stream;
    size_t cursor;
} Parser;

static inline TokenType peek(Parser *p) {
    if (p->cursor < p->stream->count) {
        return p->stream->tokens[p->cursor].type;
    }
    return TOK_EOF;
}

static inline TokenType advance(Parser *p) {
    TokenType t = peek(p);
    if (p->cursor < p->stream->count) {
        p->cursor++;
    }
    return t;
}

static bool parse_line(Parser *p);
static bool parse_arg(Parser *p);
static bool parse_cmd(Parser *p);
static bool parse_tgt(Parser *p);
static bool parse_bg(Parser *p);

static bool parse_line(Parser *p) {
    TokenType t = peek(p);
    
    if (t == TOK_EOF) {
        return true;
    }
    
    if (t == WORD) {
        advance(p);
        return parse_arg(p);
    }
    return false;
}

static bool parse_arg(Parser *p) {
    TokenType t = peek(p);

    if (t == TOK_EOF) {
        return true;
    }

    if (t == WORD) {
        advance(p);
        return parse_arg(p);
    }

    if (t == OP_LT) {
        advance(p);
        return parse_tgt(p);
    }

    if (t == OP_GT) {
        advance(p);
        return parse_tgt(p);
    }

    if (t == OP_GTGT) {
        advance(p);
        return parse_tgt(p);
    }

    if (t == OP_PIPE) {
        advance(p);
        return parse_cmd(p);
    }

    if (t == OP_SEMI) {
        advance(p);
        return parse_cmd(p);
    }

    if (t == OP_AMP) {
        advance(p);
        return parse_bg(p);
    }

    return false;
}

static bool parse_cmd(Parser *p) {
    
    if (peek(p) == WORD) {
        advance(p);
        return parse_arg(p);
    }
    return false;
}

static bool parse_tgt(Parser *p) {
    
    if (peek(p) == WORD) {
        advance(p);
        return parse_arg(p);
    }
    return false;
}

static bool parse_bg(Parser *p) {
    TokenType t = peek(p);
    
    if (t == TOK_EOF) {
        return true;
    }
    
    if (t == WORD) {
        advance(p);
        return parse_arg(p);
    }
    return false;
}

bool validate_syntax(const TokenStream *stream) {
    if (!stream) return false;

    for (size_t i = 0; i < stream->count; i++) {
        if (stream->tokens[i].type == ERROR) {
            return false;
        }
    }

    Parser parser = { .stream = stream, .cursor = 0 };

    if (!parse_line(&parser)) {
        return false;
    }

    return peek(&parser) == TOK_EOF;
}