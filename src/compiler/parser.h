// parser.h - Recursive descent parser for RHelix
#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include "token.h"
#include "ast.h"

typedef struct {
    Token** tokens;
    int token_count;
    int current;
    bool had_error;
    char error_message[256];
    int error_line;
    int error_column;
} Parser;

// Create a parser over the given token stream.
// The parser does NOT take ownership of the tokens — the caller still
// owns and must destroy them after parsing.
Parser* parser_create(Token** tokens, int token_count);
void parser_destroy(Parser* parser);

// Parse a single expression from the token stream.
// Returns NULL on error; check parser->had_error and parser->error_message.
// Caller owns the returned AST and must call ast_destroy() on it.
ASTNode* parser_parse_expression(Parser* parser);

#endif // PARSER_H
