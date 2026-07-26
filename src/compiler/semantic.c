// semantic.c - Semantic analyzer for RHelix
//
// This file walks the AST maintaining scope state. Right now it only
// validates that push/pop is balanced across the traversal - no real
// semantic checks yet. That's intentional: the goal of this foundation
// session is proving the scaffolding works before piling features on it.
//
// Future sessions will add symbol tables to each scope, name resolution
// against the scope chain, break/continue validation, return validation,
// and eventually type checking.

#include "semantic.h"
#include <stdio.h>
#include <stdlib.h>

// === Lifecycle ===

SemanticAnalyzer* semantic_create(void) {
    SemanticAnalyzer* sem = (SemanticAnalyzer*)malloc(sizeof(SemanticAnalyzer));
    if (!sem) return NULL;
    sem->current_scope = NULL;
    sem->had_error = false;
    sem->max_depth_reached = 0;
    return sem;
}

void semantic_destroy(SemanticAnalyzer* sem) {
    if (!sem) return;
    // Pop any lingering scopes (shouldn't happen if push/pop was balanced,
    // but defensive cleanup catches bugs during development).
    while (sem->current_scope) {
        scope_pop(sem);
    }
    free(sem);
}

// === Scope operations ===

void scope_push(SemanticAnalyzer* sem, ScopeKind kind) {
    if (!sem) return;
    Scope* scope = (Scope*)malloc(sizeof(Scope));
    if (!scope) return;
    scope->kind = kind;
    scope->parent = sem->current_scope;
    scope->depth = sem->current_scope ? sem->current_scope->depth + 1 : 0;
    sem->current_scope = scope;
    if (scope->depth > sem->max_depth_reached) {
        sem->max_depth_reached = scope->depth;
    }
}

void scope_pop(SemanticAnalyzer* sem) {
    if (!sem || !sem->current_scope) return;
    Scope* old = sem->current_scope;
    sem->current_scope = old->parent;
    free(old);
}

Scope* scope_current(SemanticAnalyzer* sem) {
    return sem ? sem->current_scope : NULL;
}

const char* scope_kind_to_string(ScopeKind kind) {
    switch (kind) {
        case SCOPE_MODULE: return "MODULE";
        case SCOPE_FUNCTION: return "FUNCTION";
        case SCOPE_CLASS: return "CLASS";
        case SCOPE_BLOCK: return "BLOCK";
        case SCOPE_LAMBDA: return "LAMBDA";
        default: return "UNKNOWN";
    }
}

// === AST walker ===
//
// analyze_node is the workhorse. It dispatches on node type and recurses
// into children, pushing and popping scope as it enters and exits scope-
// creating constructs. This is a preorder traversal (visit-then-recurse)
// which is the natural fit for scope tracking - you need to know you're
// in a new scope BEFORE walking the children.

static void analyze_node(SemanticAnalyzer* sem, ASTNode* node);

static void analyze_block_body(SemanticAnalyzer* sem, ASTNode* block) {
    if (!block || block->type != AST_BLOCK) return;
    for (int i = 0; i < block->as.block.count; i++) {
        analyze_node(sem, block->as.block.statements[i]);
    }
}

static void analyze_node(SemanticAnalyzer* sem, ASTNode* node) {
    if (!node || sem->had_error) return;

    switch (node->type) {
        // Leaves - no children to walk, no scope changes.
        case AST_LITERAL_INT:
        case AST_LITERAL_FLOAT:
        case AST_LITERAL_STRING:
        case AST_LITERAL_BOOL:
        case AST_LITERAL_NONE:
        case AST_IDENTIFIER:
        case AST_PASS:
        case AST_BREAK:
        case AST_CONTINUE:
            break;

        // Simple recursive expressions - walk children, no scope change.
        case AST_BINARY:
            analyze_node(sem, node->as.binary.left);
            analyze_node(sem, node->as.binary.right);
            break;
        case AST_UNARY:
            analyze_node(sem, node->as.unary.operand);
            break;
        case AST_GROUPING:
            analyze_node(sem, node->as.grouping.expression);
            break;
        case AST_CALL:
            analyze_node(sem, node->as.call.callee);
            for (int i = 0; i < node->as.call.arg_count; i++) {
                analyze_node(sem, node->as.call.args[i]);
            }
            break;
        case AST_SUBSCRIPT:
            analyze_node(sem, node->as.subscript.object);
            analyze_node(sem, node->as.subscript.index);
            break;
        case AST_ATTRIBUTE:
            analyze_node(sem, node->as.attribute.object);
            break;

        // Statements - walk children, no scope change.
        case AST_EXPRESSION_STMT:
            analyze_node(sem, node->as.expression_stmt.expression);
            break;
        case AST_ASSIGNMENT:
            analyze_node(sem, node->as.assignment.target);
            analyze_node(sem, node->as.assignment.value);
            break;
        case AST_RETURN:
            if (node->as.ret.value) {
                analyze_node(sem, node->as.ret.value);
            }
            break;

        // Scope-creating constructs. Push scope, walk children, pop scope.

        case AST_IF:
            analyze_node(sem, node->as.if_stmt.condition);
            scope_push(sem, SCOPE_BLOCK);
            analyze_block_body(sem, node->as.if_stmt.then_block);
            scope_pop(sem);
            if (node->as.if_stmt.else_block) {
                scope_push(sem, SCOPE_BLOCK);
                // else_block might be an If (elif chain) or a Block
                if (node->as.if_stmt.else_block->type == AST_BLOCK) {
                    analyze_block_body(sem, node->as.if_stmt.else_block);
                } else {
                    analyze_node(sem, node->as.if_stmt.else_block);
                }
                scope_pop(sem);
            }
            break;

        case AST_WHILE:
            analyze_node(sem, node->as.while_stmt.condition);
            scope_push(sem, SCOPE_BLOCK);
            analyze_block_body(sem, node->as.while_stmt.body);
            scope_pop(sem);
            break;

        case AST_FOR:
            analyze_node(sem, node->as.for_stmt.iterable);
            scope_push(sem, SCOPE_BLOCK);
            analyze_block_body(sem, node->as.for_stmt.body);
            scope_pop(sem);
            break;

        case AST_WITH:
            analyze_node(sem, node->as.with_stmt.context);
            scope_push(sem, SCOPE_BLOCK);
            analyze_block_body(sem, node->as.with_stmt.body);
            scope_pop(sem);
            break;

        case AST_FUNCTION_DEF:
            // Decorators are evaluated in the enclosing scope, not inside
            // the function body.
            for (int i = 0; i < node->as.function_def.decorator_count; i++) {
                analyze_node(sem, node->as.function_def.decorators[i]);
            }
            scope_push(sem, SCOPE_FUNCTION);
            analyze_block_body(sem, node->as.function_def.body);
            scope_pop(sem);
            break;

        case AST_LAMBDA:
            scope_push(sem, SCOPE_LAMBDA);
            analyze_node(sem, node->as.lambda.body);
            scope_pop(sem);
            break;

        case AST_CLASS_DEF:
            for (int i = 0; i < node->as.class_def.decorator_count; i++) {
                analyze_node(sem, node->as.class_def.decorators[i]);
            }
            scope_push(sem, SCOPE_CLASS);
            analyze_block_body(sem, node->as.class_def.body);
            scope_pop(sem);
            break;

        // Block that's not directly inside a scope-creating construct
        // (rare - most blocks are children of if/while/etc). Treat as
        // implicit block scope.
        case AST_BLOCK:
            scope_push(sem, SCOPE_BLOCK);
            analyze_block_body(sem, node);
            scope_pop(sem);
            break;

        case AST_MODULE:
            // MODULE scope is pushed by semantic_analyze, not here.
            for (int i = 0; i < node->as.module.count; i++) {
                analyze_node(sem, node->as.module.statements[i]);
            }
            break;
    }
}

// === Main entry point ===

bool semantic_analyze(SemanticAnalyzer* sem, ASTNode* module) {
    if (!sem || !module || module->type != AST_MODULE) return false;

    scope_push(sem, SCOPE_MODULE);
    analyze_node(sem, module);
    scope_pop(sem);

    // Sanity check: scope stack should be empty after balanced traversal.
    if (sem->current_scope != NULL) {
        fprintf(stderr, "[semantic] Scope stack imbalanced after analysis\n");
        sem->had_error = true;
    }

    return !sem->had_error;
}
