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
static ASTNode* pipeline(Parser* parser);
static ASTNode* logical_or(Parser* parser);
static ASTNode* logical_and(Parser* parser);
static ASTNode* equality(Parser* parser);
static ASTNode* comparison(Parser* parser);
static ASTNode* term(Parser* parser);
static ASTNode* factor(Parser* parser);
static ASTNode* unary(Parser* parser);
static ASTNode* call(Parser* parser);
static ASTNode* primary(Parser* parser);

static ASTNode* statement(Parser* parser);
static ASTNode* return_statement(Parser* parser);
static ASTNode* pass_statement(Parser* parser);
static ASTNode* break_statement(Parser* parser);
static ASTNode* continue_statement(Parser* parser);
static ASTNode* assignment_statement(Parser* parser);
static ASTNode* expression_statement(Parser* parser);
static ASTNode* block(Parser* parser);
static ASTNode* if_statement(Parser* parser);
static ASTNode* while_statement(Parser* parser);
static ASTNode* for_statement(Parser* parser);
static ASTNode* with_statement(Parser* parser);
static ASTNode* function_def_statement(Parser* parser);
static ASTNode* class_def_statement(Parser* parser);
static ASTNode* decorated_statement(Parser* parser);

// ===== Expression grammar =====

static ASTNode* expression(Parser* parser) {
    return pipeline(parser);
}

// pipeline -> logical_or ( "|>" logical_or )*
//
// Left-associative pipe. Lower precedence than logical_or so that
// "x + 1 |> double" parses as "(x + 1) |> double", not "x + (1 |> double)".
// Reuses AST_BINARY with TOKEN_PIPELINE as the operator.
static ASTNode* pipeline(Parser* parser) {
    ASTNode* left = logical_or(parser);
    if (!left) return NULL;
    while (check(parser, TOKEN_PIPELINE)) {
        Token* op = advance(parser);
        ASTNode* right = logical_or(parser);
        if (!right) { ast_destroy(left); return NULL; }
        left = ast_binary(op->type, left, right, op->line, op->column);
    }
    return left;
}
// logical_or -> logical_and ( "or" logical_and )*
static ASTNode* logical_or(Parser* parser) {
    ASTNode* left = logical_and(parser);
    if (!left) return NULL;
    while (check(parser, TOKEN_OR)) {
        Token* op = advance(parser);
        ASTNode* right = logical_and(parser);
        if (!right) { ast_destroy(left); return NULL; }
        left = ast_binary(op->type, left, right, op->line, op->column);
    }
    return left;
}

// logical_and -> equality ( "and" equality )*
static ASTNode* logical_and(Parser* parser) {
    ASTNode* left = equality(parser);
    if (!left) return NULL;
    while (check(parser, TOKEN_AND)) {
        Token* op = advance(parser);
        ASTNode* right = equality(parser);
        if (!right) { ast_destroy(left); return NULL; }
        left = ast_binary(op->type, left, right, op->line, op->column);
    }
    return left;
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
    if (check(parser, TOKEN_MINUS) || check(parser, TOKEN_NOT)) {
        Token* op = advance(parser);
        ASTNode* operand = unary(parser);
        if (!operand) return NULL;
        return ast_unary(op->type, operand, op->line, op->column);
    }
    return call(parser);
}

// call -> primary ( "(" arguments? ")" | "[" expression "]" | "." IDENT )*
static ASTNode* call(Parser* parser) {
    ASTNode* expr = primary(parser);
    if (!expr) return NULL;

    while (true) {
        if (check(parser, TOKEN_LPAREN)) {
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
        } else if (check(parser, TOKEN_LBRACKET)) {
            Token* lbracket = advance(parser);

            ASTNode* index = expression(parser);
            if (!index) {
                ast_destroy(expr);
                return NULL;
            }

            if (!consume(parser, TOKEN_RBRACKET,
                         "Expected ']' after subscript index")) {
                ast_destroy(expr);
                ast_destroy(index);
                return NULL;
            }

            ASTNode* sub = ast_subscript(expr, index,
                                         lbracket->line, lbracket->column);
            if (!sub) {
                ast_destroy(expr);
                ast_destroy(index);
                return NULL;
            }
            expr = sub;
        } else if (check(parser, TOKEN_DOT)) {
            Token* dot = advance(parser);

            if (!check(parser, TOKEN_IDENTIFIER)) {
                parser_error(parser, "Expected identifier after '.'");
                ast_destroy(expr);
                return NULL;
            }
            Token* name_tok = advance(parser);

            ASTNode* attr = ast_attribute(expr, name_tok->lexeme,
                                          dot->line, dot->column);
            if (!attr) {
                ast_destroy(expr);
                return NULL;
            }
            expr = attr;
        } else {
            break;
        }
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
        // Check for unparenthesized single-param lambda: x => body
        if (check(parser, TOKEN_FAT_ARROW)) {
            advance(parser);  // Consume =>
            ASTNode* body = expression(parser);
            if (!body) return NULL;
            ASTNode* lambda = ast_lambda(body, token->line, token->column);
            if (!lambda) { ast_destroy(body); return NULL; }
            ast_lambda_add_param(lambda, token->lexeme);
            return lambda;
        }
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

// statement -> @ decorated | class | def | if | while | for | return | pass
//            | assignment | expression_stmt
static ASTNode* statement(Parser* parser) {
    if (check(parser, TOKEN_AT))     return decorated_statement(parser);
    if (check(parser, TOKEN_CLASS))  return class_def_statement(parser);
    if (check(parser, TOKEN_DEF))    return function_def_statement(parser);
    if (check(parser, TOKEN_IF))     return if_statement(parser);
    if (check(parser, TOKEN_WHILE))  return while_statement(parser);
    if (check(parser, TOKEN_FOR))    return for_statement(parser);
    if (check(parser, TOKEN_WITH))   return with_statement(parser);
    if (check(parser, TOKEN_RETURN)) return return_statement(parser);
    if (check(parser, TOKEN_PASS))   return pass_statement(parser);
    if (check(parser, TOKEN_BREAK))    return break_statement(parser);
    if (check(parser, TOKEN_CONTINUE)) return continue_statement(parser);

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

// pass_statement -> "pass" NEWLINE?
static ASTNode* pass_statement(Parser* parser) {
    Token* pass_token = advance(parser);
    match(parser, TOKEN_NEWLINE);
    return ast_pass(pass_token->line, pass_token->column);
}

// break_statement -> "break" NEWLINE?
static ASTNode* break_statement(Parser* parser) {
    Token* break_token = advance(parser);
    match(parser, TOKEN_NEWLINE);
    return ast_break(break_token->line, break_token->column);
}

// continue_statement -> "continue" NEWLINE?
static ASTNode* continue_statement(Parser* parser) {
    Token* continue_token = advance(parser);
    match(parser, TOKEN_NEWLINE);
    return ast_continue(continue_token->line, continue_token->column);
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
// expression_statement -> expression ( "=" expression )? NEWLINE?
//
// We parse the left side as a full expression, then check if "=" follows.
// If yes, the expression must be a valid assignment target (Identifier,
// Attribute, or Subscript) and we build an Assignment node. If no, the
// expression stands alone as an ExpressionStmt. This pattern matches how
// Python and most production parsers handle assignment - parse-then-classify
// rather than predict-then-parse.
static ASTNode* expression_statement(Parser* parser) {
    Token* token = peek(parser);
    ASTNode* expr = expression(parser);
    if (!expr) return NULL;

    if (match(parser, TOKEN_EQUALS)) {
        if (expr->type != AST_IDENTIFIER &&
            expr->type != AST_ATTRIBUTE &&
            expr->type != AST_SUBSCRIPT) {
            parser_error(parser, "Invalid assignment target");
            ast_destroy(expr);
            return NULL;
        }
        ASTNode* value = expression(parser);
        if (!value) {
            ast_destroy(expr);
            return NULL;
        }
        match(parser, TOKEN_NEWLINE);
        return ast_assignment(expr, value, token->line, token->column);
    }

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
    if (check(parser, TOKEN_ELIF)) {
        // elif is sugar: parse it as a nested If and wrap in an
        // implicit else-block of the current If. The recursive call
        // to if_statement handles the rest of the chain.
        else_block = if_statement(parser);
        if (!else_block) {
            ast_destroy(condition);
            ast_destroy(then_block);
            return NULL;
        }
    } else if (check(parser, TOKEN_ELSE)) {
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
    Token* for_token = advance(parser);

    if (!check(parser, TOKEN_IDENTIFIER)) {
        parser_error(parser, "Expected identifier after 'for'");
        return NULL;
    }
    Token* var_token = advance(parser);

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

// with_statement -> "with" expression ( "as" IDENTIFIER )? ":" NEWLINE block
//
// The context expression is anything the expression parser produces - a call
// like arena(1024), an identifier like resource, an attribute like self.pool.
// The 'as' binding is optional; when absent, var_name is NULL. The body is
// a regular indented block.
static ASTNode* with_statement(Parser* parser) {
    Token* with_token = advance(parser);  // Consume WITH

    ASTNode* context = expression(parser);
    if (!context) return NULL;

    char* var_name = NULL;
    if (match(parser, TOKEN_AS)) {
        if (!check(parser, TOKEN_IDENTIFIER)) {
            parser_error(parser, "Expected identifier after 'as'");
            ast_destroy(context);
            return NULL;
        }
        Token* name_tok = advance(parser);
        var_name = name_tok->lexeme;  // ast_with will strdup this
    }

    if (!consume(parser, TOKEN_COLON, "Expected ':' after with context")) {
        ast_destroy(context);
        return NULL;
    }
    if (!consume(parser, TOKEN_NEWLINE, "Expected newline after ':'")) {
        ast_destroy(context);
        return NULL;
    }

    ASTNode* body = block(parser);
    if (!body) {
        ast_destroy(context);
        return NULL;
    }

    return ast_with(context, var_name, body,
                    with_token->line, with_token->column);
}

// Helper: parse a single parameter "IDENT ( : IDENT )?"
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
static ASTNode* function_def_statement(Parser* parser) {
    Token* def_token = advance(parser);

    if (!check(parser, TOKEN_IDENTIFIER)) {
        parser_error(parser, "Expected function name after 'def'");
        return NULL;
    }
    Token* name_token = advance(parser);

    if (!consume(parser, TOKEN_LPAREN, "Expected '(' after function name")) {
        return NULL;
    }

    ASTNode* func = ast_function_def(name_token->lexeme, NULL, NULL,
                                     def_token->line, def_token->column);
    if (!func) return NULL;

    if (!check(parser, TOKEN_RPAREN)) {
        char* p_name;
        ASTNode* p_type;
        if (!parse_param(parser, &p_name, &p_type)) {
            ast_destroy(func);
            return NULL;
        }
        ast_function_def_add_param(func, p_name, p_type);
        free(p_name);

        while (check(parser, TOKEN_COMMA)) {
            advance(parser);
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

// class_def_statement -> "class" IDENT ( "(" IDENT ("," IDENT)* ")" )?
//                        ":" NEWLINE block
static ASTNode* class_def_statement(Parser* parser) {
    Token* class_token = advance(parser);

    if (!check(parser, TOKEN_IDENTIFIER)) {
        parser_error(parser, "Expected class name after 'class'");
        return NULL;
    }
    Token* name_token = advance(parser);

    ASTNode* cls = ast_class_def(name_token->lexeme, NULL,
                                 class_token->line, class_token->column);
    if (!cls) return NULL;

    if (match(parser, TOKEN_LPAREN)) {
        if (!check(parser, TOKEN_RPAREN)) {
            if (!check(parser, TOKEN_IDENTIFIER)) {
                parser_error(parser, "Expected base class name");
                ast_destroy(cls);
                return NULL;
            }
            Token* base_tok = advance(parser);
            ast_class_def_add_base(cls, base_tok->lexeme,
                                   base_tok->line, base_tok->column);

            while (check(parser, TOKEN_COMMA)) {
                advance(parser);
                if (!check(parser, TOKEN_IDENTIFIER)) {
                    parser_error(parser, "Expected base class name after ','");
                    ast_destroy(cls);
                    return NULL;
                }
                Token* next_base = advance(parser);
                ast_class_def_add_base(cls, next_base->lexeme,
                                       next_base->line, next_base->column);
            }
        }

        if (!consume(parser, TOKEN_RPAREN, "Expected ')' after base classes")) {
            ast_destroy(cls);
            return NULL;
        }
    }

    if (!consume(parser, TOKEN_COLON, "Expected ':' after class header")) {
        ast_destroy(cls);
        return NULL;
    }
    if (!consume(parser, TOKEN_NEWLINE, "Expected newline after ':'")) {
        ast_destroy(cls);
        return NULL;
    }

    ASTNode* body = block(parser);
    if (!body) {
        ast_destroy(cls);
        return NULL;
    }
    cls->as.class_def.body = body;

    return cls;
}

// decorated_statement -> ( "@" call NEWLINE )+ ( function_def | class_def )
//
// A decorator is "@" followed by any call-layer expression. That gives us
// @name, @name(args), @module.name, and @module.name(args) for free because
// we just call our existing call() function. Decorators stack: any number
// of "@..." lines may precede a def or class. After collecting them all,
// we parse the following def/class and attach the decorators to it.
static ASTNode* decorated_statement(Parser* parser) {
    ASTNode** decorators = NULL;
    int decorator_count = 0;
    int decorator_capacity = 0;

    while (check(parser, TOKEN_AT)) {
        advance(parser);  // Consume @

        ASTNode* dec = call(parser);
        if (!dec) {
            for (int i = 0; i < decorator_count; i++) ast_destroy(decorators[i]);
            free(decorators);
            return NULL;
        }

        if (!consume(parser, TOKEN_NEWLINE, "Expected newline after decorator")) {
            ast_destroy(dec);
            for (int i = 0; i < decorator_count; i++) ast_destroy(decorators[i]);
            free(decorators);
            return NULL;
        }

        if (decorator_count >= decorator_capacity) {
            int new_cap = decorator_capacity == 0 ? 4 : decorator_capacity * 2;
            decorators = (ASTNode**)realloc(decorators,
                                            sizeof(ASTNode*) * new_cap);
            decorator_capacity = new_cap;
        }
        decorators[decorator_count++] = dec;

        skip_newlines(parser);
    }

    ASTNode* target = NULL;
    if (check(parser, TOKEN_DEF)) {
        target = function_def_statement(parser);
    } else if (check(parser, TOKEN_CLASS)) {
        target = class_def_statement(parser);
    } else {
        parser_error(parser, "Expected 'def' or 'class' after decorator");
        for (int i = 0; i < decorator_count; i++) ast_destroy(decorators[i]);
        free(decorators);
        return NULL;
    }

    if (!target) {
        for (int i = 0; i < decorator_count; i++) ast_destroy(decorators[i]);
        free(decorators);
        return NULL;
    }

    // Attach decorators. Ownership transfers from the temporary array to the
    // target node. We free the array shell but not the elements.
    for (int i = 0; i < decorator_count; i++) {
        if (target->type == AST_FUNCTION_DEF) {
            ast_function_def_add_decorator(target, decorators[i]);
        } else {
            ast_class_def_add_decorator(target, decorators[i]);
        }
    }
    free(decorators);

    return target;
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
