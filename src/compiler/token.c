// token.c - Token implementation
#include "token.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Token* token_create(TokenType type, const char* lexeme, int line, int column) {
    Token* token = (Token*)malloc(sizeof(Token));
    if (!token) return NULL;

    token->type = type;
    token->lexeme = lexeme ? strdup(lexeme) : NULL;
    token->line = line;
    token->column = column;

    token->value.int_value = 0;

    return token;
}

void token_destroy(Token* token) {
    if (!token) return;
    if (token->lexeme) free(token->lexeme);
    if (token->type == TOKEN_STRING && token->value.string_value) {
        free(token->value.string_value);
    }
    free(token);
}

const char* token_type_to_string(TokenType type) {
    switch (type) {
        // Literals
        case TOKEN_INT: return "INT";
        case TOKEN_FLOAT: return "FLOAT";
        case TOKEN_STRING: return "STRING";
        case TOKEN_IDENTIFIER: return "IDENTIFIER";

        // Keywords
        case TOKEN_DEF: return "DEF";
        case TOKEN_RETURN: return "RETURN";
        case TOKEN_IF: return "IF";
        case TOKEN_ELSE: return "ELSE";
        case TOKEN_FOR: return "FOR";
        case TOKEN_IN: return "IN";
        case TOKEN_WHILE: return "WHILE";
        case TOKEN_CLASS: return "CLASS";
        case TOKEN_IMPORT: return "IMPORT";
        case TOKEN_TRUE: return "TRUE";
        case TOKEN_FALSE: return "FALSE";
        case TOKEN_NONE: return "NONE";

        // Memory keywords
        case TOKEN_WITH: return "WITH";
        case TOKEN_ARENA: return "ARENA";
        case TOKEN_STACK: return "STACK";
        case TOKEN_ALLOC: return "ALLOC";
        case TOKEN_MOVE: return "MOVE";
        case TOKEN_OWNED: return "OWNED";
        case TOKEN_WEAK: return "WEAK";

        // Operators
        case TOKEN_PLUS: return "PLUS";
        case TOKEN_MINUS: return "MINUS";
        case TOKEN_STAR: return "STAR";
        case TOKEN_SLASH: return "SLASH";
        case TOKEN_PERCENT: return "PERCENT";
        case TOKEN_EQUALS: return "EQUALS";
        case TOKEN_EQUALS_EQUALS: return "EQUALS_EQUALS";
        case TOKEN_NOT_EQUALS: return "NOT_EQUALS";
        case TOKEN_LESS: return "LESS";
        case TOKEN_GREATER: return "GREATER";
        case TOKEN_LESS_EQUALS: return "LESS_EQUALS";
        case TOKEN_GREATER_EQUALS: return "GREATER_EQUALS";
        case TOKEN_ARROW: return "ARROW";
        case TOKEN_PIPE: return "PIPE";
        case TOKEN_FAT_ARROW: return "FAT_ARROW";

        // Delimiters
        case TOKEN_LPAREN: return "LPAREN";
        case TOKEN_RPAREN: return "RPAREN";
        case TOKEN_LBRACKET: return "LBRACKET";
        case TOKEN_RBRACKET: return "RBRACKET";
        case TOKEN_LBRACE: return "LBRACE";
        case TOKEN_RBRACE: return "RBRACE";
        case TOKEN_COMMA: return "COMMA";
        case TOKEN_DOT: return "DOT";
        case TOKEN_COLON: return "COLON";
        case TOKEN_SEMICOLON: return "SEMICOLON";
        case TOKEN_AT: return "AT";

        // Special
        case TOKEN_NEWLINE: return "NEWLINE";
        case TOKEN_INDENT: return "INDENT";
        case TOKEN_DEDENT: return "DEDENT";
        case TOKEN_EOF: return "EOF";
        case TOKEN_ERROR: return "ERROR";

        default: return "UNKNOWN";
    }
}

void token_print(Token* token) {
    // For newline tokens, show a placeholder so output stays on one line
    if (token->type == TOKEN_NEWLINE) {
        printf("Token(NEWLINE, '\\n', %d:%d)", token->line, token->column);
        return;
    }
    printf("Token(%s, '%s', %d:%d)",
           token_type_to_string(token->type),
           token->lexeme ? token->lexeme : "",
           token->line,
           token->column);
}
