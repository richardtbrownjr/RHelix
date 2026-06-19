// parser.c - Recursive descent parser for RHelix
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ===== Lifecycle =====

Parser* parser_create(Token** tokens, int token_count) {
    Parser* parser = (Parser*)malloc(sizeof(Parser));
    if (!parser) return NULL;
    parser->tokens = tokens;
    parser->token_count = token_count;
    parser->current = 0;
    parser->had_error = false;
    parser->error_message[0] = '\0';
    parser->error_line = 0;
    parser->error_column = 0;
    return parser;
}

void parser_destroy(Parser* parser) {
    if (!parser) return;
    free(parser);
}

// ===== Internal helpers =====

static Token* peek(Parser* parser) {
    return parser->tokens[parser->current];
}

static Token* previous(Parser* parser) {
    return parser->tokens[parser->current - 1];
}

static bool is_at_end(Parser* parser) {
    return peek(parser)->type == TOKEN_EOF;
}

static Token* advance(Parser* parser) {
    if (!is_at_end(parser)) {
        parser->current++;
    }
    return previous(parser);
}

static bool check(Parser* parser, TokenType type) {
    if (is_at_end(parser)) return false;
    return peek(parser)->type == type;
}

static bool match(Parser* parser, TokenType type) {
    if (check(parser, type)) {
        advance(parser);
        return true;
    }
    return false;
}

static void parser_error(Parser* parser, const char* message) {
    if (parser->had_error) return;  // First error wins; don't overwrite
    parser->had_error = true;
    Token* token = peek(parser);
    snprintf(parser->error_message, sizeof(parser->error_message),
             "[line %d, col %d] Parse error: %s (at '%s')",
             token->line, token->column, message,
             token->lexeme ? token->lexeme : "");
    parser->error_line = token->line;
    parser->error_column = token->column;
}

static Token* consume(Parser* parser, TokenType type, const char* message) {
    if (check(parser, type)) {
        return advance(parser);
    }
    parser_error(parser, message);
    return NULL;
}

// ===== Forward declarations for the precedence chain =====

static ASTNode* expression(Parser* parser);
static ASTNode* equality(Parser* parser);
static ASTNode* comparison(Parser* parser);
static ASTNode* term(Parser* parser);
static ASTNode* factor(Parser* parser);
static ASTNode* unary(Parser* parser);
static ASTNode* primary(Parser* parser);

// ===== Grammar (lowest to highest precedence) =====

// expression -> equality
static ASTNode* expression(Parser* parser) {
    return equality(parser);
}

// equality -> comparison ( ( "==" | "!=" ) comparison )*
static ASTNode* equality(Parser* parser) {
    ASTNode* left = comparison(parser);
    if (!left) return NULL;
    while (check(parser, TOKEN_EQUALS_EQUALS) || check(parser, TOKEN_NOT_EQUALS)) {
        Token* op = advance(parser);
        ASTNode* right = comparison(parser);
        if (!right) {
            ast_destroy(left);
            return NULL;
        }
        left = ast_binary(op->type, left, right, op->line, op->column);
    }
    return left;
}

// comparison -> term ( ( "<" | ">" | "<=" | ">=" ) term )*
static ASTNode* comparison(Parser* parser) {
    ASTNode* left = term(parser);
    if (!left) return NULL;
    while (check(parser, TOKEN_LESS) || check(parser, TOKEN_GREATER) ||
           check(parser, TOKEN_LESS_EQUALS) || check(parser, TOKEN_GREATER_EQUALS)) {
        Token* op = advance(parser);
        ASTNode* right = term(parser);
        if (!right) {
            ast_destroy(left);
            return NULL;
        }
        left = ast_binary(op->type, left, right, op->line, op->column);
    }
    return left;
}

// term -> factor ( ( "+" | "-" ) factor )*
static ASTNode* term(Parser* parser) {
    ASTNode* left = factor(parser);
    if (!left) return NULL;
    while (check(parser, TOKEN_PLUS) || check(parser, TOKEN_MINUS)) {
        Token* op = advance(parser);
        ASTNode* right = factor(parser);
        if (!right) {
            ast_destroy(left);
            return NULL;
        }
        left = ast_binary(op->type, left, right, op->line, op->column);
    }
    return left;
}

// factor -> unary ( ( "*" | "/" | "%" ) unary )*
static ASTNode* factor(Parser* parser) {
    ASTNode* left = unary(parser);
    if (!left) return NULL;
    while (check(parser, TOKEN_STAR) || check(parser, TOKEN_SLASH) ||
           check(parser, TOKEN_PERCENT)) {
        Token* op = advance(parser);
        ASTNode* right = unary(parser);
        if (!right) {
            ast_destroy(left);
            return NULL;
        }
        left = ast_binary(op->type, left, right, op->line, op->column);
    }
    return left;
}

// unary -> "-" unary | primary
static ASTNode* unary(Parser* parser) {
    if (check(parser, TOKEN_MINUS)) {
        Token* op = advance(parser);
        ASTNode* operand = unary(parser);  // Right-recursive: "- - x" => -(-x)
        if (!operand) return NULL;
        return ast_unary(op->type, operand, op->line, op->column);
    }
    return primary(parser);
}

// primary -> INT | FLOAT | STRING | TRUE | FALSE | NONE | IDENTIFIER
//          | "(" expression ")"
static ASTNode* primary(Parser* parser) {
    Token* token = peek(parser);

    if (match(parser, TOKEN_INT)) {
        return ast_literal_int(token->value.int_value, token->line, token->column);
    }
    if (match(parser, TOKEN_FLOAT)) {
        return ast_literal_float(token->value.float_value, token->line, token->column);
    }
    if (match(parser, TOKEN_STRING)) {
        return ast_literal_string(token->value.string_value, token->line, token->column);
    }
    if (match(parser, TOKEN_TRUE)) {
        return ast_literal_bool(1, token->line, token->column);
    }
    if (match(parser, TOKEN_FALSE)) {
        return ast_literal_bool(0, token->line, token->column);
    }
    if (match(parser, TOKEN_NONE)) {
        return ast_literal_none(token->line, token->column);
    }
    if (match(parser, TOKEN_IDENTIFIER)) {
        return ast_identifier(token->lexeme, token->line, token->column);
    }
    if (match(parser, TOKEN_LPAREN)) {
        ASTNode* expr = expression(parser);
        if (!expr) return NULL;
        if (!consume(parser, TOKEN_RPAREN, "Expected ')' after expression")) {
            ast_destroy(expr);
            return NULL;
        }
        return ast_grouping(expr, token->line, token->column);
    }

    parser_error(parser, "Expected expression");
    return NULL;
}

// ===== Public API =====

ASTNode* parser_parse_expression(Parser* parser) {
    if (!parser || parser->had_error) return NULL;
    return expression(parser);
}
