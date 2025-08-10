// lexer.c - Lexical analyzer implementation
#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

// Keyword table
typedef struct {
    const char* keyword;
    TokenType type;
} Keyword;

static Keyword keywords[] = {
    {"def", TOKEN_DEF},
    {"return", TOKEN_RETURN},
    {"if", TOKEN_IF},
    {"else", TOKEN_ELSE},
    {"for", TOKEN_FOR},
    {"while", TOKEN_WHILE},
    {"class", TOKEN_CLASS},
    {"import", TOKEN_IMPORT},
    {"with", TOKEN_WITH},
    {"arena", TOKEN_ARENA},
    {"stack", TOKEN_STACK},
    {"alloc", TOKEN_ALLOC},
    {"move", TOKEN_MOVE},
    {"owned", TOKEN_OWNED},
    {"weak", TOKEN_WEAK},
    {"True", TOKEN_TRUE},
    {"False", TOKEN_FALSE},
    {"None", TOKEN_NONE},
    {NULL, 0}
};

Lexer* lexer_create(const char* source) {
    Lexer* lexer = (Lexer*)malloc(sizeof(Lexer));
    if (!lexer) return NULL;

    lexer->source = source;
    lexer->current = source;
    lexer->line = 1;
    lexer->column = 1;
    lexer->indent_level = 0;

    // Initialize indent stack
    lexer->indent_stack_size = 32;
    lexer->indent_stack = (int*)malloc(sizeof(int) * lexer->indent_stack_size);
    lexer->indent_stack[0] = 0;
    lexer->indent_stack_top = 0;

    return lexer;
}

void lexer_destroy(Lexer* lexer) {
    if (!lexer) return;
    free(lexer->indent_stack);
    free(lexer);
}

// Helper functions
static bool is_at_end(Lexer* lexer) {
    return *lexer->current == '\0';
}

static char peek(Lexer* lexer) {
    return *lexer->current;
}

static char peek_next(Lexer* lexer) {
    if (is_at_end(lexer)) return '\0';
    return lexer->current[1];
}

static char advance(Lexer* lexer) {
    char c = *lexer->current;
    if (c == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    lexer->current++;
    return c;
}

static bool match(Lexer* lexer, char expected) {
    if (is_at_end(lexer)) return false;
    if (*lexer->current != expected) return false;
    advance(lexer);
    return true;
}

static void skip_whitespace(Lexer* lexer) {
    while (!is_at_end(lexer)) {
        char c = peek(lexer);
        if (c == ' ' || c == '\r' || c == '\t') {
            advance(lexer);
        } else if (c == '#') {
            // Skip comment
            while (peek(lexer) != '\n' && !is_at_end(lexer)) {
                advance(lexer);
            }
        } else {
            break;
        }
    }
}

static Token* make_token(Lexer* lexer, TokenType type, const char* start) {
    int length = (int)(lexer->current - start);
    char* lexeme = (char*)malloc(length + 1);
    strncpy(lexeme, start, length);
    lexeme[length] = '\0';

    Token* token = token_create(type, lexeme, lexer->line, lexer->column - length);
    free(lexeme);
    return token;
}

static Token* make_error(Lexer* lexer, const char* message) {
    return token_create(TOKEN_ERROR, message, lexer->line, lexer->column);
}

static Token* read_string(Lexer* lexer) {
    const char* start = lexer->current;
    char quote = advance(lexer); // Consume opening quote

    while (!is_at_end(lexer) && peek(lexer) != quote) {
        if (peek(lexer) == '\\') {
            advance(lexer); // Consume backslash
            if (!is_at_end(lexer)) advance(lexer); // Consume escaped char
        } else {
            advance(lexer);
        }
    }

    if (is_at_end(lexer)) {
        return make_error(lexer, "Unterminated string");
    }

    advance(lexer); // Consume closing quote

    // Create string token
    int length = (int)(lexer->current - start - 2); // Exclude quotes
    char* value = (char*)malloc(length + 1);
    strncpy(value, start + 1, length);
    value[length] = '\0';

    Token* token = make_token(lexer, TOKEN_STRING, start);
    token->value.string_value = value;
    return token;
}

static Token* read_number(Lexer* lexer) {
    const char* start = lexer->current;
    bool is_float = false;

    while (isdigit(peek(lexer))) {
        advance(lexer);
    }

    if (peek(lexer) == '.' && isdigit(peek_next(lexer))) {
        is_float = true;
        advance(lexer); // Consume '.'
        while (isdigit(peek(lexer))) {
            advance(lexer);
        }
    }

    Token* token = make_token(lexer, is_float ? TOKEN_FLOAT : TOKEN_INT, start);

    if (is_float) {
        token->value.float_value = strtod(start, NULL);
    } else {
        token->value.int_value = strtol(start, NULL, 10);
    }

    return token;
}

static Token* read_identifier(Lexer* lexer) {
    const char* start = lexer->current;

    while (isalnum(peek(lexer)) || peek(lexer) == '_') {
        advance(lexer);
    }

    // Check if it's a keyword
    int length = (int)(lexer->current - start);
    for (int i = 0; keywords[i].keyword != NULL; i++) {
        if (strlen(keywords[i].keyword) == length &&
            strncmp(keywords[i].keyword, start, length) == 0) {
            return make_token(lexer, keywords[i].type, start);
        }
    }

    return make_token(lexer, TOKEN_IDENTIFIER, start);
}

Token* lexer_next_token(Lexer* lexer) {
    skip_whitespace(lexer);

    if (is_at_end(lexer)) {
        return token_create(TOKEN_EOF, "", lexer->line, lexer->column);
    }

    const char* start = lexer->current;
    char c = advance(lexer);

    // Single character tokens
    switch (c) {
        case '(': return make_token(lexer, TOKEN_LPAREN, start);
        case ')': return make_token(lexer, TOKEN_RPAREN, start);
        case '[': return make_token(lexer, TOKEN_LBRACKET, start);
        case ']': return make_token(lexer, TOKEN_RBRACKET, start);
        case '{': return make_token(lexer, TOKEN_LBRACE, start);
        case '}': return make_token(lexer, TOKEN_RBRACE, start);
        case ',': return make_token(lexer, TOKEN_COMMA, start);
        case '.': return make_token(lexer, TOKEN_DOT, start);
        case ':': return make_token(lexer, TOKEN_COLON, start);
        case ';': return make_token(lexer, TOKEN_SEMICOLON, start);
        case '@': return make_token(lexer, TOKEN_AT, start);
        case '+': return make_token(lexer, TOKEN_PLUS, start);
        case '*': return make_token(lexer, TOKEN_STAR, start);
        case '/': return make_token(lexer, TOKEN_SLASH, start);
        case '%': return make_token(lexer, TOKEN_PERCENT, start);

        // Multi-character tokens
        case '-':
            if (match(lexer, '>')) {
                return make_token(lexer, TOKEN_ARROW, start);
            }
            return make_token(lexer, TOKEN_MINUS, start);

        case '=':
            if (match(lexer, '=')) {
                return make_token(lexer, TOKEN_EQUALS_EQUALS, start);
            }
            return make_token(lexer, TOKEN_EQUALS, start);

        case '!':
            if (match(lexer, '=')) {
                return make_token(lexer, TOKEN_NOT_EQUALS, start);
            }
            return make_error(lexer, "Unexpected character '!'");

        case '<':
            if (match(lexer, '=')) {
                return make_token(lexer, TOKEN_LESS_EQUALS, start);
            }
            return make_token(lexer, TOKEN_LESS, start);

        case '>':
            if (match(lexer, '=')) {
                return make_token(lexer, TOKEN_GREATER_EQUALS, start);
            }
            return make_token(lexer, TOKEN_GREATER, start);

        case '|':
            if (match(lexer, '>')) {
                return make_token(lexer, TOKEN_PIPE, start);
            }
            return make_error(lexer, "Unexpected character '|'");

        case '"':
        case '\'':
            lexer->current = start; // Back up to include quote
            return read_string(lexer);

        case '\n':
            // Handle newlines for Python-like syntax
            return make_token(lexer, TOKEN_NEWLINE, start);
    }

    // Numbers
    if (isdigit(c)) {
        lexer->current = start; // Back up
        return read_number(lexer);
    }

    // Identifiers and keywords
    if (isalpha(c) || c == '_') {
        lexer->current = start; // Back up
        return read_identifier(lexer);
    }

    return make_error(lexer, "Unexpected character");
}

Token** lexer_tokenize(const char* source, int* token_count) {
    Lexer* lexer = lexer_create(source);
    if (!lexer) return NULL;

    // Dynamic array of tokens
    int capacity = 100;
    Token** tokens = (Token**)malloc(sizeof(Token*) * capacity);
    int count = 0;

    Token* token;
    do {
        token = lexer_next_token(lexer);

        if (count >= capacity) {
            capacity *= 2;
            tokens = (Token**)realloc(tokens, sizeof(Token*) * capacity);
        }

        tokens[count++] = token;
    } while (token->type != TOKEN_EOF && token->type != TOKEN_ERROR);

    *token_count = count;
    lexer_destroy(lexer);
    return tokens;
}
