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
    
    // Initialize value union
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
        case TOKEN_INT: return "INT";
        case TOKEN_FLOAT: return "FLOAT";
        case TOKEN_STRING: return "STRING";
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TOKEN_DEF: return "DEF";
        case TOKEN_RETURN: return "RETURN";
        case TOKEN_IF: return "IF";
        case TOKEN_ELSE: return "ELSE";
        case TOKEN_FOR: return "FOR";
        case TOKEN_WHILE: return "WHILE";
        case TOKEN_WITH: return "WITH";
        case TOKEN_ARENA: return "ARENA";
        case TOKEN_STACK: return "STACK";
        case TOKEN_ALLOC: return "ALLOC";
        case TOKEN_PLUS: return "PLUS";
        case TOKEN_MINUS: return "MINUS";
        case TOKEN_STAR: return "STAR";
        case TOKEN_SLASH: return "SLASH";
        case TOKEN_EQUALS: return "EQUALS";
        case TOKEN_LPAREN: return "LPAREN";
        case TOKEN_RPAREN: return "RPAREN";
        case TOKEN_EOF: return "EOF";
        case TOKEN_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

void token_print(Token* token) {
    printf("Token(%s, '%s', %d:%d)", 
           token_type_to_string(token->type),
           token->lexeme ? token->lexeme : "",
           token->line,
           token->column);
}
