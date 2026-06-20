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

static Token* peek_at(Parser* parser, int offset) {
    int idx = parser->current + offset;
    if (idx < 0 || idx >= parser->token_count) return NULL;
    return parser->tokens[idx];
}

static Token* previous(Parser* parser) {
    return parser->tokens[parser->current - 1];
}

static bool is_at_end(Parser* parser) {
    return peek(parser)->type == TOKEN_EOF;
}

static Token* advance(Parser* parser) {
    if (!is_at_end(parser)) parser->current++;
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

static void skip_newlines(Parser* parser) {
    while (check(parser, TOKEN_NEWLINE)) advance(parser);
}

static void parser_error(Parser* parser, const char* message) {
    if (parser->had_error) return;
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
    if (check(parser, type)) return advance(parser);
    parser_error(parser, message);
    return NULL;
}

// ===== Forward declarations =====

static ASTNode* expression(Parser* parser);
static ASTNode* equality(Parser* parser);
static ASTNode* comparison(Parser* parser);
static ASTNode* term(Parser* parser);
static ASTNode* factor(Parser* parser);
static ASTNode* unary(Parser* parser);
static ASTNode* call(Parser* parser);
static ASTNode* primary(Parser* parser);

static ASTNode* statement(Parser* parser);
static ASTNode* return_statement(Parser* parser);
static ASTNode* assignment_statement(Parser* parser);
static ASTNode* expression_statement(Parser* parser);
static ASTNode* block(Parser* parser);
static ASTNode* if_statement(Parser* parser);
static ASTNode* while_statement(Parser* parser);
static ASTNode* for_statement(Parser* parser);
static ASTNode* function_def_statement(Parser* parser);

// ===== Expression grammar =====

static ASTNode* expression(Parser* parser) {
    return equality(parser);
}

static ASTNode* equality(Parser* parser) {
    ASTNode* left = comparison(parser);
    if (!left) return NULL;
    while (check(parser, TOKEN_EQUALS_EQUALS) || check(parser, TOKEN_NOT_EQUALS)) {
        Token* op = advance(parser);
        ASTNode* right = comparison(parser);
        if (!right) { ast_destroy(left); return NULL; }
        left = ast_binary(op->type, left, right, op->line, op->column);
    }
    return left;
}

static ASTNode* comparison(Parser* parser) {
    ASTNode* left = term(parser);
    if (!left) return NULL;
    while (check(parser, TOKEN_LESS) || check(parser, TOKEN_GREATER) ||
           check(parser, TOKEN_LESS_EQUALS) || check(parser, TOKEN_GREATER_EQUALS)) {
        Token* op = advance(parser);
        ASTNode* right = term(parser);
        if (!right) { ast_destroy(left); return NULL; }
        left = ast_binary(op->type, left, right, op->line, op->column);
    }
    return left;
}

static ASTNode* term(Parser* parser) {
    ASTNode* left = factor(parser);
    if (!left) return NULL;
    while (check(parser, TOKEN_PLUS) || check(parser, TOKEN_MINUS)) {
        Token* op = advance(parser);
        ASTNode* right = factor(parser);
        if (!right) { ast_destroy(left); return NULL; }
        left = ast_binary(op->type, left, right, op->line, op->column);
    }
    return left;
}

static ASTNode* factor(Parser* parser) {
    ASTNode* left = unary(parser);
    if (!left) return NULL;
    while (check(parser, TOKEN_STAR) || check(parser, TOKEN_SLASH) ||
           check(parser, TOKEN_PERCENT)) {
        Token* op = advance(parser);
        ASTNode* right = unary(parser);
        if (!right) { ast_destroy(left); return NULL; }
        left = ast_binary(op->type, left, right, op->line, op->column);
    }
    return left;
}

static ASTNode* unary(Parser* parser) {
    if (check(parser, TOKEN_MINUS)) {
        Token* op = advance(parser);
        ASTNode* operand = unary(parser);
        if (!operand) return NULL;
        return ast_unary(op->type, operand, op->line, op->column);
    }
    return call(parser);
}

// call -> primary ( "(" arguments? ")" )*
// arguments -> expression ( "," expression )*
static ASTNode* call(Parser* parser) {
    ASTNode* expr = primary(parser);
    if (!expr) return NULL;

    while (check(parser, TOKEN_LPAREN)) {
        Token* lparen = advance(parser);

        ASTNode* call_node = ast_call(expr, lparen->line, lparen->column);
        if (!call_node) {
            ast_destroy(expr);
            return NULL;
        }

        if (!check(parser, TOKEN_RPAREN)) {
            ASTNode* arg = expression(parser);
            if (!arg) {
                ast_destroy(call_node);
                return NULL;
            }
            ast_call_add_arg(call_node, arg);

            while (check(parser, TOKEN_COMMA)) {
                advance(parser);
                arg = expression(parser);
                if (!arg) {
                    ast_destroy(call_node);
                    return NULL;
                }
                ast_call_add_arg(call_node, arg);
            }
        }

        if (!consume(parser, TOKEN_RPAREN, "Expected ')' after arguments")) {
            ast_destroy(call_node);
            return NULL;
        }

        expr = call_node;
    }

    return expr;
}

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

// ===== Statement grammar =====

// statement -> def | if | while | for | return | assignment | expression_stmt
static ASTNode* statement(Parser* parser) {
    if (check(parser, TOKEN_DEF))    return function_def_statement(parser);
    if (check(parser, TOKEN_IF))     return if_statement(parser);
    if (check(parser, TOKEN_WHILE))  return while_statement(parser);
    if (check(parser, TOKEN_FOR))    return for_statement(parser);
    if (check(parser, TOKEN_RETURN)) return return_statement(parser);
    if (check(parser, TOKEN_IDENTIFIER)) {
        Token* next = peek_at(parser, 1);
        if (next && next->type == TOKEN_EQUALS) {
            return assignment_statement(parser);
        }
    }
    return expression_statement(parser);
}

// return_statement -> "return" expression? NEWLINE?
static ASTNode* return_statement(Parser* parser) {
    Token* return_token = advance(parser);
    ASTNode* value = NULL;
    if (!check(parser, TOKEN_NEWLINE) && !is_at_end(parser)) {
        value = expression(parser);
        if (!value) return NULL;
    }
    match(parser, TOKEN_NEWLINE);
    return ast_return(value, return_token->line, return_token->column);
}

// assignment_statement -> IDENTIFIER "=" expression NEWLINE?
static ASTNode* assignment_statement(Parser* parser) {
    Token* name_token = advance(parser);
    advance(parser);  // Consume EQUALS (verified by lookahead)
    ASTNode* value = expression(parser);
    if (!value) return NULL;
    match(parser, TOKEN_NEWLINE);
    ASTNode* target = ast_identifier(name_token->lexeme,
                                     name_token->line, name_token->column);
    return ast_assignment(target, value,
                          name_token->line, name_token->column);
}

// expression_statement -> expression NEWLINE?
static ASTNode* expression_statement(Parser* parser) {
    Token* token = peek(parser);
    ASTNode* expr = expression(parser);
    if (!expr) return NULL;
    match(parser, TOKEN_NEWLINE);
    return ast_expression_stmt(expr, token->line, token->column);
}

// block -> INDENT (NEWLINE | statement)* DEDENT
static ASTNode* block(Parser* parser) {
    Token* indent_token = peek(parser);
    if (!consume(parser, TOKEN_INDENT, "Expected indented block")) return NULL;

    ASTNode* blk = ast_block(indent_token->line, indent_token->column);
    if (!blk) return NULL;

    skip_newlines(parser);
    while (!check(parser, TOKEN_DEDENT) && !is_at_end(parser) && !parser->had_error) {
        ASTNode* stmt = statement(parser);
        if (!stmt) {
            ast_destroy(blk);
            return NULL;
        }
        ast_block_add(blk, stmt);
        skip_newlines(parser);
    }

    if (!consume(parser, TOKEN_DEDENT, "Expected dedent at end of block")) {
        ast_destroy(blk);
        return NULL;
    }
    return blk;
}

// if_statement -> "if" expression ":" NEWLINE block ( "else" ":" NEWLINE block )?
static ASTNode* if_statement(Parser* parser) {
    Token* if_token = advance(parser);

    ASTNode* condition = expression(parser);
    if (!condition) return NULL;

    if (!consume(parser, TOKEN_COLON, "Expected ':' after if condition")) {
        ast_destroy(condition);
        return NULL;
    }
    if (!consume(parser, TOKEN_NEWLINE, "Expected newline after ':'")) {
        ast_destroy(condition);
        return NULL;
    }

    ASTNode* then_block = block(parser);
    if (!then_block) {
        ast_destroy(condition);
        return NULL;
    }

    ASTNode* else_block = NULL;
    if (check(parser, TOKEN_ELSE)) {
        advance(parser);
        if (!consume(parser, TOKEN_COLON, "Expected ':' after else")) {
            ast_destroy(condition);
            ast_destroy(then_block);
            return NULL;
        }
        if (!consume(parser, TOKEN_NEWLINE, "Expected newline after ':'")) {
            ast_destroy(condition);
            ast_destroy(then_block);
            return NULL;
        }
        else_block = block(parser);
        if (!else_block) {
            ast_destroy(condition);
            ast_destroy(then_block);
            return NULL;
        }
    }

    return ast_if(condition, then_block, else_block,
                  if_token->line, if_token->column);
}

// while_statement -> "while" expression ":" NEWLINE block
static ASTNode* while_statement(Parser* parser) {
    Token* while_token = advance(parser);

    ASTNode* condition = expression(parser);
    if (!condition) return NULL;

    if (!consume(parser, TOKEN_COLON, "Expected ':' after while condition")) {
        ast_destroy(condition);
        return NULL;
    }
    if (!consume(parser, TOKEN_NEWLINE, "Expected newline after ':'")) {
        ast_destroy(condition);
        return NULL;
    }

    ASTNode* body = block(parser);
    if (!body) {
        ast_destroy(condition);
        return NULL;
    }

    return ast_while(condition, body, while_token->line, while_token->column);
}

// for_statement -> "for" IDENTIFIER "in" expression ":" NEWLINE block
static ASTNode* for_statement(Parser* parser) {
    Token* for_token = advance(parser);  // Consume FOR

    if (!check(parser, TOKEN_IDENTIFIER)) {
        parser_error(parser, "Expected identifier after 'for'");
        return NULL;
    }
    Token* var_token = advance(parser);  // Consume IDENTIFIER

    if (!consume(parser, TOKEN_IN, "Expected 'in' after for variable")) {
        return NULL;
    }

    ASTNode* iterable = expression(parser);
    if (!iterable) return NULL;

    if (!consume(parser, TOKEN_COLON, "Expected ':' after for iterable")) {
        ast_destroy(iterable);
        return NULL;
    }
    if (!consume(parser, TOKEN_NEWLINE, "Expected newline after ':'")) {
        ast_destroy(iterable);
        return NULL;
    }

    ASTNode* body = block(parser);
    if (!body) {
        ast_destroy(iterable);
        return NULL;
    }

    return ast_for(var_token->lexeme, iterable, body,
                   for_token->line, for_token->column);
}

// Helper: parse a single parameter "IDENT ( : IDENT )?"
// On success, *out_name owns a strdup'd name and *out_type owns an identifier
// node (or NULL). On failure, *out_name and *out_type are NULL.
static bool parse_param(Parser* parser, char** out_name, ASTNode** out_type) {
    *out_name = NULL;
    *out_type = NULL;
    if (!check(parser, TOKEN_IDENTIFIER)) {
        parser_error(parser, "Expected parameter name");
        return false;
    }
    Token* name_tok = advance(parser);
    *out_name = strdup(name_tok->lexeme);

    if (match(parser, TOKEN_COLON)) {
        if (!check(parser, TOKEN_IDENTIFIER)) {
            parser_error(parser, "Expected type after ':'");
            free(*out_name);
            *out_name = NULL;
            return false;
        }
        Token* type_tok = advance(parser);
        *out_type = ast_identifier(type_tok->lexeme,
                                   type_tok->line, type_tok->column);
    }
    return true;
}

// function_def_statement -> "def" IDENT "(" params? ")" ( "->" IDENT )?
//                           ":" NEWLINE block
// params -> param ( "," param )*
// param  -> IDENT ( ":" IDENT )?
static ASTNode* function_def_statement(Parser* parser) {
    Token* def_token = advance(parser);  // Consume DEF

    if (!check(parser, TOKEN_IDENTIFIER)) {
        parser_error(parser, "Expected function name after 'def'");
        return NULL;
    }
    Token* name_token = advance(parser);

    if (!consume(parser, TOKEN_LPAREN, "Expected '(' after function name")) {
        return NULL;
    }

    // Build the FunctionDef node early so ast_destroy handles cleanup on any
    // error path below. body and return_type stay NULL until parsed.
    ASTNode* func = ast_function_def(name_token->lexeme, NULL, NULL,
                                     def_token->line, def_token->column);
    if (!func) return NULL;

    // Parameter list (possibly empty)
    if (!check(parser, TOKEN_RPAREN)) {
        char* p_name;
        ASTNode* p_type;
        if (!parse_param(parser, &p_name, &p_type)) {
            ast_destroy(func);
            return NULL;
        }
        ast_function_def_add_param(func, p_name, p_type);
        free(p_name);  // add_param strdup's; free our local copy

        while (check(parser, TOKEN_COMMA)) {
            advance(parser);  // consume ,
            if (!parse_param(parser, &p_name, &p_type)) {
                ast_destroy(func);
                return NULL;
            }
            ast_function_def_add_param(func, p_name, p_type);
            free(p_name);
        }
    }

    if (!consume(parser, TOKEN_RPAREN, "Expected ')' after parameters")) {
        ast_destroy(func);
        return NULL;
    }

    // Optional return type annotation
    if (match(parser, TOKEN_ARROW)) {
        if (!check(parser, TOKEN_IDENTIFIER)) {
            parser_error(parser, "Expected return type after '->'");
            ast_destroy(func);
            return NULL;
        }
        Token* ret_tok = advance(parser);
        func->as.function_def.return_type =
            ast_identifier(ret_tok->lexeme, ret_tok->line, ret_tok->column);
    }

    if (!consume(parser, TOKEN_COLON, "Expected ':' after function signature")) {
        ast_destroy(func);
        return NULL;
    }
    if (!consume(parser, TOKEN_NEWLINE, "Expected newline after ':'")) {
        ast_destroy(func);
        return NULL;
    }

    ASTNode* body = block(parser);
    if (!body) {
        ast_destroy(func);
        return NULL;
    }
    func->as.function_def.body = body;

    return func;
}

// ===== Public API =====

ASTNode* parser_parse_expression(Parser* parser) {
    if (!parser || parser->had_error) return NULL;
    return expression(parser);
}

ASTNode* parser_parse_module(Parser* parser) {
    if (!parser || parser->had_error) return NULL;

    Token* first = parser->tokens[0];
    ASTNode* module = ast_module(first->line, first->column);
    if (!module) return NULL;

    skip_newlines(parser);
    while (!is_at_end(parser) && !parser->had_error) {
        ASTNode* stmt = statement(parser);
        if (!stmt) break;
        ast_module_add(module, stmt);
        skip_newlines(parser);
    }

    if (parser->had_error) {
        ast_destroy(module);
        return NULL;
    }
    return module;
}
