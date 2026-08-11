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
#include <string.h>
#include <stdarg.h>

// === Lifecycle ===

SemanticAnalyzer* semantic_create(void) {
    SemanticAnalyzer* sem = (SemanticAnalyzer*)malloc(sizeof(SemanticAnalyzer));
    if (!sem) return NULL;
    sem->current_scope = NULL;
    sem->had_error = false;
    sem->error_count = 0;
    sem->max_depth_reached = 0;
    sem->debug_print_scopes = false;
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
    scope->symbol_table = NULL;  // Empty table on scope creation
    sem->current_scope = scope;
    if (scope->depth > sem->max_depth_reached) {
        sem->max_depth_reached = scope->depth;
    }
}

void scope_pop(SemanticAnalyzer* sem) {
    if (!sem || !sem->current_scope) return;
    Scope* old = sem->current_scope;
    // Debug output: print the scope's contents before we free it.
    if (sem->debug_print_scopes) {
        printf("  Scope [%s @ depth %d] symbols:\n",
               scope_kind_to_string(old->kind), old->depth);
        if (!old->symbol_table) {
            printf("    (empty)\n");
        }
        for (Symbol* s = old->symbol_table; s; s = s->next) {
            printf("    %-10s %s (line %d)\n",
                   symbol_kind_to_string(s->kind), s->name, s->defined_line);
        }
    }
    // Free the symbol table before freeing the scope itself.
    Symbol* sym = old->symbol_table;
    while (sym) {
        Symbol* next = sym->next;
        free(sym->name);
        free(sym);
        sym = next;
    }
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

// === Symbol operations ===

Symbol* symbol_define(SemanticAnalyzer* sem, const char* name, SymbolKind kind,
                      int line, int column) {
    if (!sem || !sem->current_scope || !name) return NULL;

    Symbol* sym = (Symbol*)malloc(sizeof(Symbol));
    if (!sym) return NULL;

    sym->name = strdup(name);
    sym->kind = kind;
    sym->defined_line = line;
    sym->defined_column = column;

    // Prepend to the current scope's symbol table. Prepending is O(1) and
    // gives us the "newer symbols shadow older ones" behavior for free
    // because lookup walks from head to tail.
    sym->next = sem->current_scope->symbol_table;
    sem->current_scope->symbol_table = sym;

    return sym;
}

Symbol* symbol_lookup_local(SemanticAnalyzer* sem, const char* name) {
    if (!sem || !sem->current_scope || !name) return NULL;
    for (Symbol* s = sem->current_scope->symbol_table; s; s = s->next) {
        if (strcmp(s->name, name) == 0) return s;
    }
    return NULL;
}

Symbol* symbol_lookup(SemanticAnalyzer* sem, const char* name) {
    if (!sem || !name) return NULL;
    // Walk from the current scope outward through parent scopes until
    // we find the name or exhaust the chain.
    for (Scope* scope = sem->current_scope; scope; scope = scope->parent) {
        for (Symbol* s = scope->symbol_table; s; s = s->next) {
            if (strcmp(s->name, name) == 0) return s;
        }
    }
    return NULL;
}

const char* symbol_kind_to_string(SymbolKind kind) {
    switch (kind) {
        case SYM_VARIABLE: return "VARIABLE";
        case SYM_PARAMETER: return "PARAMETER";
        case SYM_FUNCTION: return "FUNCTION";
        case SYM_CLASS: return "CLASS";
        case SYM_METHOD: return "METHOD";
        default: return "UNKNOWN";
    }
    }

    // === Error reporting ===

void semantic_error(SemanticAnalyzer* sem, int line, int column,
                    const char* format, ...) {
    if (!sem) return;
    sem->had_error = true;
    sem->error_count++;

    fprintf(stderr, "[semantic] line %d, col %d: ", line, column);

    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "\n");
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
        case AST_PASS:
        case AST_BREAK:
        case AST_CONTINUE:
            break;

      case AST_IDENTIFIER: {
          // Identifier appearing in expression position - look up in the
          // scope chain. Assignment targets are pre-defined by AST_ASSIGNMENT
          // before recursion reaches here, so lookup will succeed for them.
          const char* name = node->as.identifier.name;
          if (name && !symbol_lookup(sem, name)) {
              semantic_error(sem, node->line, node->column,
                             "undefined name '%s'", name);
          }
          break;
      }

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
          // If the assignment target is a plain identifier, this is a
          // variable definition. Attribute and subscript targets don't
          // define new names - they mutate existing objects.
          if (node->as.assignment.target &&
              node->as.assignment.target->type == AST_IDENTIFIER) {
              symbol_define(sem,
                            node->as.assignment.target->as.identifier.name,
                            SYM_VARIABLE,
                            node->line, node->column);
          }
            analyze_node(sem, node->as.assignment.target);
            analyze_node(sem, node->as.assignment.value);
            break;
        case AST_AUGMENTED_ASSIGNMENT:
          // Augmented assignment does NOT define - target must already
          // exist. Walking the target will trigger name resolution and
          // report an error if the name is undefined.
          analyze_node(sem, node->as.augmented_assignment.target);
          analyze_node(sem, node->as.augmented_assignment.value);
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
            // Loop variable is defined in the loop's block scope.
            if (node->as.for_stmt.var_name) {
                symbol_define(sem, node->as.for_stmt.var_name,
                              SYM_VARIABLE, node->line, node->column);
            }
            analyze_block_body(sem, node->as.for_stmt.body);
            scope_pop(sem);
            break;

        case AST_WITH:
            analyze_node(sem, node->as.with_stmt.context);
            scope_push(sem, SCOPE_BLOCK);
            // 'as' binding (if present) is defined in the with-block scope.
            if (node->as.with_stmt.var_name) {
                symbol_define(sem, node->as.with_stmt.var_name,
                              SYM_VARIABLE, node->line, node->column);
            }
            analyze_block_body(sem, node->as.with_stmt.body);
            scope_pop(sem);
            break;

        case AST_FUNCTION_DEF:
          for (int i = 0; i < node->as.function_def.decorator_count; i++) {
                analyze_node(sem, node->as.function_def.decorators[i]);
            }
            // Define the function/method in the ENCLOSING scope. Whether
            // this is SYM_FUNCTION or SYM_METHOD depends on whether we're
            // currently inside a class body.
            if (node->as.function_def.name) {
                SymbolKind fk = (sem->current_scope &&
                                 sem->current_scope->kind == SCOPE_CLASS)
                                ? SYM_METHOD : SYM_FUNCTION;
                symbol_define(sem, node->as.function_def.name,
                              fk, node->line, node->column);
            }
            scope_push(sem, SCOPE_FUNCTION);
            // Parameters live in the function's own scope.
            for (int i = 0; i < node->as.function_def.param_count; i++) {
                symbol_define(sem, node->as.function_def.params[i].name,
                              SYM_PARAMETER, node->line, node->column);
            }
            analyze_block_body(sem, node->as.function_def.body);
            scope_pop(sem);
            break;

      case AST_LAMBDA:
        scope_push(sem, SCOPE_LAMBDA);
        // Lambda parameters live in the lambda's own scope.
        for (int i = 0; i < node->as.lambda.param_count; i++) {
            symbol_define(sem, node->as.lambda.param_names[i],
                          SYM_PARAMETER, node->line, node->column);
        }
        analyze_node(sem, node->as.lambda.body);
        scope_pop(sem);
        break;

        case AST_CLASS_DEF:
            for (int i = 0; i < node->as.class_def.decorator_count; i++) {
                analyze_node(sem, node->as.class_def.decorators[i]);
            }
            // Define the class in the enclosing scope.
            if (node->as.class_def.name) {
                symbol_define(sem, node->as.class_def.name,
                              SYM_CLASS, node->line, node->column);
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
