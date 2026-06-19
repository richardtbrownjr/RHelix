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

Parser* parser_create(Token** tokens, int token_count);
void parser_destroy(Parser* parser);

// Parse a single expression. Caller owns the returned AST.
ASTNode* parser_parse_expression(Parser* parser);

// Parse a module (sequence of statements). Caller owns the returned AST.
ASTNode* parser_parse_module(Parser* parser);

#endif // PARSER_H
