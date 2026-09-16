// evaluator.c - Tree-walking interpreter for RHelix expressions

#include "evaluator.h"
#include "../compiler/token.h"
#include "../compiler/ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// === Helpers ===

static Value runtime_error(ASTNode* node, const char* format, ...) {
    fprintf(stderr, "[runtime] line %d, col %d: ",
            node ? node->line : 0,
            node ? node->column : 0);
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
    return value_none();
}

static bool is_numeric(Value v) {
    return v.kind == VAL_INT || v.kind == VAL_FLOAT;
}

static double to_double(Value v) {
    return v.kind == VAL_INT ? (double)v.as.i : v.as.f;
}

static bool is_truthy(Value v) {
    switch (v.kind) {
        case VAL_NONE:   return false;
        case VAL_BOOL:   return v.as.b;
        case VAL_INT:    return v.as.i != 0;
        case VAL_FLOAT:  return v.as.f != 0.0;
        case VAL_STRING: return v.as.string.length > 0;
        case VAL_FUNCTION: return v.as.function != NULL;
        default:         return true;
    }
}

// Shorthand for building a StmtResult that has NOT returned.
static StmtResult stmt_ok(Value v) {
    StmtResult r;
    r.value = v;
    r.returned = false;
    return r;
}

// Shorthand for building a StmtResult that HAS returned.
static StmtResult stmt_return(Value v) {
    StmtResult r;
    r.value = v;
    r.returned = true;
    return r;
}

// === Binary arithmetic ===

static Value eval_arithmetic(ASTNode* node, Value left, Value right) {
    TokenType op = node->as.binary.op;

    if (left.kind == VAL_STRING && right.kind == VAL_STRING) {
        if (op == TOKEN_PLUS) {
            int total = left.as.string.length + right.as.string.length;
            char* buf = (char*)malloc(total + 1);
            if (!buf) return value_none();
            memcpy(buf, left.as.string.chars, left.as.string.length);
            memcpy(buf + left.as.string.length,
                   right.as.string.chars, right.as.string.length);
            buf[total] = '\0';
            Value result;
            result.kind = VAL_STRING;
            result.as.string.chars = buf;
            result.as.string.length = total;
            return result;
        }
        return runtime_error(node, "unsupported string operation");
    }

    if (!is_numeric(left) || !is_numeric(right)) {
        return runtime_error(node,
            "cannot apply arithmetic to non-numeric values");
    }

    bool promote = (left.kind == VAL_FLOAT || right.kind == VAL_FLOAT);

    if (promote) {
        double l = to_double(left);
        double r = to_double(right);
        switch (op) {
            case TOKEN_PLUS:    return value_float(l + r);
            case TOKEN_MINUS:   return value_float(l - r);
            case TOKEN_STAR:    return value_float(l * r);
            case TOKEN_SLASH:
                if (r == 0.0) return runtime_error(node, "division by zero");
                return value_float(l / r);
            case TOKEN_PERCENT:
                if (r == 0.0) return runtime_error(node, "modulo by zero");
                return value_float(l - r * ((long long)(l / r)));
            default: return runtime_error(node, "unsupported arithmetic op");
        }
    }

    long long l = left.as.i;
    long long r = right.as.i;
    switch (op) {
        case TOKEN_PLUS:    return value_int(l + r);
        case TOKEN_MINUS:   return value_int(l - r);
        case TOKEN_STAR:    return value_int(l * r);
        case TOKEN_SLASH:
            if (r == 0) return runtime_error(node, "division by zero");
            return value_int(l / r);
        case TOKEN_PERCENT:
            if (r == 0) return runtime_error(node, "modulo by zero");
            return value_int(l % r);
        default: return runtime_error(node, "unsupported arithmetic op");
    }
}

// === Binary comparison ===

static Value eval_comparison(ASTNode* node, Value left, Value right) {
    TokenType op = node->as.binary.op;

    if (is_numeric(left) && is_numeric(right)) {
        double l = to_double(left);
        double r = to_double(right);
        switch (op) {
            case TOKEN_LESS:            return value_bool(l < r);
            case TOKEN_GREATER:         return value_bool(l > r);
            case TOKEN_LESS_EQUALS:     return value_bool(l <= r);
            case TOKEN_GREATER_EQUALS:  return value_bool(l >= r);
            case TOKEN_EQUALS_EQUALS:   return value_bool(l == r);
            case TOKEN_NOT_EQUALS:      return value_bool(l != r);
            default: return runtime_error(node, "unsupported comparison op");
        }
    }

    if (left.kind == VAL_STRING && right.kind == VAL_STRING) {
        if (op == TOKEN_EQUALS_EQUALS) return value_bool(value_equals(left, right));
        if (op == TOKEN_NOT_EQUALS)    return value_bool(!value_equals(left, right));
        return runtime_error(node, "cannot order strings with <, >, <=, >=");
    }

    if (op == TOKEN_EQUALS_EQUALS) return value_bool(value_equals(left, right));
    if (op == TOKEN_NOT_EQUALS)    return value_bool(!value_equals(left, right));

    return runtime_error(node, "cannot compare values of incompatible types");
}

// === Binary logical (short-circuit) ===

static Value eval_logical(ASTNode* node, Environment* env) {
    TokenType op = node->as.binary.op;
    Value left = evaluate(node->as.binary.left, env);

    if (op == TOKEN_AND) {
        if (!is_truthy(left)) return left;
        value_destroy(&left);
        return evaluate(node->as.binary.right, env);
    }
    if (op == TOKEN_OR) {
        if (is_truthy(left)) return left;
        value_destroy(&left);
        return evaluate(node->as.binary.right, env);
    }
    value_destroy(&left);
    return runtime_error(node, "unsupported logical op");
}

// === Unary ===

static Value eval_unary(ASTNode* node, Environment* env) {
    TokenType op = node->as.unary.op;
    Value operand = evaluate(node->as.unary.operand, env);

    if (op == TOKEN_NOT) {
        bool result = !is_truthy(operand);
        value_destroy(&operand);
        return value_bool(result);
    }

    if (op == TOKEN_MINUS) {
        if (operand.kind == VAL_INT) {
            Value r = value_int(-operand.as.i);
            value_destroy(&operand);
            return r;
        }
        if (operand.kind == VAL_FLOAT) {
            Value r = value_float(-operand.as.f);
            value_destroy(&operand);
            return r;
        }
        value_destroy(&operand);
        return runtime_error(node, "unary '-' requires numeric operand");
    }

    if (op == TOKEN_PLUS) {
        if (is_numeric(operand)) return operand;
        value_destroy(&operand);
        return runtime_error(node, "unary '+' requires numeric operand");
    }

    value_destroy(&operand);
    return runtime_error(node, "unsupported unary op");
}

// === Function calls ===
// Evaluates a call: extracts the callable, evaluates arguments,
// creates a child environment with parameters bound, executes the
// body, unwinds on return.

static Value eval_call(ASTNode* node, Environment* env) {
    // Evaluate the callee expression
    Value callee = evaluate(node->as.call.callee, env);

    if (callee.kind != VAL_FUNCTION || !callee.as.function) {
        value_destroy(&callee);
        return runtime_error(node, "value is not callable");
    }

    FunctionValue* fn = callee.as.function;
    ASTNode* def = fn->definition;

    // Check arity: parameter count vs argument count
    int expected = def->as.function_def.param_count;
    int actual = node->as.call.arg_count;
    if (expected != actual) {
        value_destroy(&callee);
        return runtime_error(node,
            "%s() takes %d arguments but %d were given",
            fn->name ? fn->name : "<anonymous>", expected, actual);
    }

    // Create child environment with closure as parent
    Environment* call_env = env_create(fn->closure);
    if (!call_env) {
        value_destroy(&callee);
        return runtime_error(node, "could not create call environment");
    }

    // Evaluate each argument and bind to the corresponding parameter
    for (int i = 0; i < actual; i++) {
        Value arg_val = evaluate(node->as.call.args[i], env);
        const char* param_name = def->as.function_def.params[i].name;
        env_define(call_env, param_name, arg_val);
    }

    // Execute the body as a block
    StmtResult body_result = evaluate_statement(def->as.function_def.body, call_env);

    // Value from return, or VAL_NONE if fell off the end
    Value return_value = body_result.returned
        ? body_result.value
        : value_none();

    // If body_result.value was NOT the return value (i.e., no return
    // fired but last statement evaluated), destroy it to avoid leak.
    // (When body_result.returned is true, we're using its value as
    // return_value so we DON'T destroy it.)
    if (!body_result.returned) {
        value_destroy(&body_result.value);
    }

    // Destroy the call environment (parameters and locals are freed here)
    env_destroy(call_env);

    // Destroy the callee Value (it was a clone anyway - FunctionValue
    // itself lives on)
    value_destroy(&callee);

    return return_value;
}

// === Main expression dispatch ===

Value evaluate(ASTNode* node, Environment* env) {
    if (!node) return value_none();

    switch (node->type) {
        // === Literals ===
        case AST_LITERAL_INT:
            return value_int(node->as.literal_int.value);
        case AST_LITERAL_FLOAT:
            return value_float(node->as.literal_float.value);
        case AST_LITERAL_STRING:
            return value_string(node->as.literal_string.value);
        case AST_LITERAL_BOOL:
            return value_bool(node->as.literal_bool.value);
        case AST_LITERAL_NONE:
            return value_none();

        // === Identifier (variable lookup) ===
        case AST_IDENTIFIER: {
            Value* stored = env_get(env, node->as.identifier.name);
            if (!stored) {
                return runtime_error(node,
                    "undefined variable '%s'",
                    node->as.identifier.name);
            }
            return value_clone(*stored);
        }

        // === Grouping ===
        case AST_GROUPING:
            return evaluate(node->as.grouping.expression, env);

        // === Binary ===
        case AST_BINARY: {
            TokenType op = node->as.binary.op;

            if (op == TOKEN_AND || op == TOKEN_OR) {
                return eval_logical(node, env);
            }

            Value left = evaluate(node->as.binary.left, env);
            Value right = evaluate(node->as.binary.right, env);
            Value result;

            if (op == TOKEN_LESS || op == TOKEN_GREATER ||
                op == TOKEN_LESS_EQUALS || op == TOKEN_GREATER_EQUALS ||
                op == TOKEN_EQUALS_EQUALS || op == TOKEN_NOT_EQUALS) {
                result = eval_comparison(node, left, right);
            }
            else if (op == TOKEN_PLUS || op == TOKEN_MINUS ||
                     op == TOKEN_STAR || op == TOKEN_SLASH ||
                     op == TOKEN_PERCENT) {
                result = eval_arithmetic(node, left, right);
            }
            else {
                result = runtime_error(node, "unsupported binary operator");
            }

            value_destroy(&left);
            value_destroy(&right);
            return result;
        }

        // === Unary ===
        case AST_UNARY:
            return eval_unary(node, env);

        // === Ternary (x if cond else y) ===
        case AST_TERNARY: {
            Value cond = evaluate(node->as.ternary.condition, env);
            bool truthy = is_truthy(cond);
            value_destroy(&cond);
            return truthy
                ? evaluate(node->as.ternary.then_expr, env)
                : evaluate(node->as.ternary.else_expr, env);
        }

        // === Function call ===
        case AST_CALL:
            return eval_call(node, env);

        // === Deferred to future sessions ===
        case AST_ATTRIBUTE:
            return runtime_error(node, "attribute access not yet supported");
        case AST_SUBSCRIPT:
            return runtime_error(node, "subscripts not yet supported");
        case AST_LIST_LITERAL:
        case AST_DICT_LITERAL:
            return runtime_error(node, "collection literals not yet supported");
        case AST_LAMBDA:
            return runtime_error(node, "lambdas not yet supported (Session 4+)");

        default:
            return runtime_error(node, "unsupported AST node type");
    }
}

// === Statement evaluation ===
//
// Returns StmtResult { value, returned }. If returned is true, a
// return statement fired and the value should propagate up. Callers
// (block iteration, function calls) check this flag and unwind.

StmtResult evaluate_statement(ASTNode* node, Environment* env) {
    if (!node) return stmt_ok(value_none());

    switch (node->type) {
        // === Expression statement ===
        case AST_EXPRESSION_STMT:
            return stmt_ok(evaluate(node->as.expression_stmt.expression, env));

        // === Assignment ===
        case AST_ASSIGNMENT: {
            ASTNode* target = node->as.assignment.target;
            if (target->type != AST_IDENTIFIER) {
                return stmt_ok(runtime_error(node,
                    "assignment target must be an identifier (Session 5 for attributes/subscripts)"));
            }
            Value rhs = evaluate(node->as.assignment.value, env);
            env_assign(env, target->as.identifier.name, rhs);
            return stmt_ok(value_none());
        }

        // === If statement ===
        case AST_IF: {
            Value cond = evaluate(node->as.if_stmt.condition, env);
            bool truthy = is_truthy(cond);
            value_destroy(&cond);

            if (truthy) {
                if (node->as.if_stmt.then_block) {
                    return evaluate_statement(node->as.if_stmt.then_block, env);
                }
            } else if (node->as.if_stmt.else_block) {
                return evaluate_statement(node->as.if_stmt.else_block, env);
            }
            return stmt_ok(value_none());
        }

        // === Return statement ===
        case AST_RETURN: {
            Value ret_val = node->as.ret.value
                ? evaluate(node->as.ret.value, env)
                : value_none();
            return stmt_return(ret_val);
        }

        // === Function definition ===
        case AST_FUNCTION_DEF: {
            // Allocate a FunctionValue capturing the current env as closure
            FunctionValue* fn = (FunctionValue*)malloc(sizeof(FunctionValue));
            if (!fn) {
                return stmt_ok(runtime_error(node, "could not allocate function value"));
            }
            fn->definition = node;
            fn->closure = env;
            fn->name = node->as.function_def.name
                ? strdup(node->as.function_def.name)
                : strdup("<anonymous>");

            // Wrap in a Value and store in current environment under
            // the function's name.
            Value fn_val = value_function(fn);
            if (node->as.function_def.name) {
                env_define(env, node->as.function_def.name, fn_val);
            } else {
                // Anonymous - shouldn't happen for FUNCTION_DEF, but be safe
                value_destroy(&fn_val);
            }
            return stmt_ok(value_none());
        }

        // === Module: execute statements in order, propagate returns ===
        case AST_MODULE: {
            Value last = value_none();
            for (int i = 0; i < node->as.module.count; i++) {
                value_destroy(&last);
                StmtResult r = evaluate_statement(node->as.module.statements[i], env);
                if (r.returned) {
                    // Return escaping the module (shouldn't happen in normal code)
                    return r;
                }
                last = r.value;
            }
            return stmt_ok(last);
        }

        // === Block: execute statements in order, propagate returns ===
        case AST_BLOCK: {
            Value last = value_none();
            for (int i = 0; i < node->as.block.count; i++) {
                value_destroy(&last);
                StmtResult r = evaluate_statement(node->as.block.statements[i], env);
                if (r.returned) {
                    return r;  // Propagate up to function call site
                }
                last = r.value;
            }
            return stmt_ok(last);
        }

        // === Pass ===
        case AST_PASS:
            return stmt_ok(value_none());

        // === Deferred to future sessions ===
        case AST_WHILE:
            return stmt_ok(runtime_error(node,
                "while loops not yet supported (Session 5)"));
        case AST_FOR:
            return stmt_ok(runtime_error(node,
                "for loops not yet supported (Session 5)"));
        case AST_CLASS_DEF:
            return stmt_ok(runtime_error(node,
                "class definitions not yet supported (Session 5)"));

        default:
            // Bare expression as statement
            return stmt_ok(evaluate(node, env));
    }
}
