// lexer.h - Lexical analyzer for RHelix
#ifndef LEXER_H
#define LEXER_H

#include "token.h"

typedef struct {
    const char* source;     // Source code
    const char* current;    // Current position
    int line;
    int column;
    int indent_level;       // For Python-like indentation
    int* indent_stack;      // Stack of indentation levels
    int indent_stack_top;
    int indent_stack_size;
} Lexer;

// Lexer creation and destruction
Lexer* lexer_create(const char* source);
void lexer_destroy(Lexer* lexer);

// Main lexing function
Token* lexer_next_token(Lexer* lexer);

// Helper to lex entire source
Token** lexer_tokenize(const char* source, int* token_count);

#endif // LEXER_H
