// token.h - Token definitions for RHelix
#ifndef TOKEN_H
#define TOKEN_H

typedef enum {
    // Literals
    TOKEN_INT,
    TOKEN_FLOAT,
    TOKEN_STRING,
    TOKEN_IDENTIFIER,

    // Keywords
    TOKEN_DEF,
    TOKEN_RETURN,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_FOR,
    TOKEN_WHILE,
    TOKEN_CLASS,
    TOKEN_IMPORT,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NONE,

    // Memory keywords (RHelix specific)
    TOKEN_WITH,
    TOKEN_ARENA,
    TOKEN_STACK,
    TOKEN_ALLOC,
    TOKEN_MOVE,
    TOKEN_OWNED,
    TOKEN_WEAK,

    // Operators
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,
    TOKEN_EQUALS,
    TOKEN_EQUALS_EQUALS,
    TOKEN_NOT_EQUALS,
    TOKEN_LESS,
    TOKEN_GREATER,
    TOKEN_LESS_EQUALS,
    TOKEN_GREATER_EQUALS,
    TOKEN_ARROW,        // ->
    TOKEN_PIPE,         // |>

    // Delimiters
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_COMMA,
    TOKEN_DOT,
    TOKEN_COLON,
    TOKEN_SEMICOLON,
    TOKEN_AT,           // @ for decorators

    // Special
    TOKEN_NEWLINE,
    TOKEN_INDENT,
    TOKEN_DEDENT,
    TOKEN_EOF,
    TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char* lexeme;       // The actual text
    union {
        long int_value;
        double float_value;
        char* string_value;
    } value;
    int line;
    int column;
} Token;

// Token creation and destruction
Token* token_create(TokenType type, const char* lexeme, int line, int column);
void token_destroy(Token* token);
const char* token_type_to_string(TokenType type);
void token_print(Token* token);

#endif // TOKEN_H
