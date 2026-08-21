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
    sem->warning_count = 0;
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
        type_destroy(sym->type);
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
        case SCOPE_LOOP_BODY: return "LOOP_BODY";
        case SCOPE_LAMBDA: return "LAMBDA";
        default: return "UNKNOWN";
    }
}

// === Symbol operations ===

Symbol* symbol_define(SemanticAnalyzer* sem, const char* name, SymbolKind kind,
                      int line, int column) {
    if (!sem || !sem->current_scope || !name) return NULL;

    // Redeclaration warning: if a function, method, or class is being defined
    // and a symbol with the same name (of ANY kind) already exists in the
    // current scope, that's almost always a bug. We don't warn on variable
    // redefinition because that's normal reassignment.
    if (kind == SYM_FUNCTION || kind == SYM_METHOD || kind == SYM_CLASS) {
        Symbol* existing = symbol_lookup_local(sem, name);
        if (existing) {
            semantic_warning(sem, line, column,
                             "redeclaration of '%s' (previously defined at line %d)",
                             name, existing->defined_line);
        }
    }

    Symbol* sym = (Symbol*)malloc(sizeof(Symbol));
    if (!sym) return NULL;

    sym->name = strdup(name);
    sym->kind = kind;
    sym->defined_line = line;
    sym->defined_column = column;
    sym->param_count = -1;  // Default: not applicable; set for SYM_FUNCTION/METHOD in walker
    sym->type = NULL;  // Type populated by walker after symbol_define returns

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


    // is_inside_loop - Returns true if the current scope chain includes at least
    // one enclosing SCOPE_LOOP_BODY. Used by break/continue validation.
    //
    // Design note: we STOP walking outward if we hit a FUNCTION, LAMBDA, or CLASS
    // scope. A break/continue inside a function nested inside a loop should NOT
    // see the outer loop - Python semantics. Example:
    //     while True:
    //         def foo():
    //             break   # ERROR - break does not escape into the outer loop
    static bool is_inside_loop(SemanticAnalyzer* sem) {
        if (!sem) return false;
        for (Scope* s = sem->current_scope; s; s = s->parent) {
            if (s->kind == SCOPE_LOOP_BODY) return true;
            if (s->kind == SCOPE_FUNCTION ||
                s->kind == SCOPE_LAMBDA   ||
                s->kind == SCOPE_CLASS) {
                return false;
            }
        }
        return false;
    }

    // is_inside_function - Returns true if the current scope chain includes
    // at least one enclosing SCOPE_FUNCTION or SCOPE_LAMBDA. Used by return
    // statement validation.
    //
    // Unlike is_inside_loop (which stops at function boundaries), return
    // simply targets the innermost function-or-lambda. First hit wins; we
    // walk outward until we find one or exhaust the chain.
    static bool is_inside_function(SemanticAnalyzer* sem) {
        if (!sem) return false;
        for (Scope* s = sem->current_scope; s; s = s->parent) {
            if (s->kind == SCOPE_FUNCTION || s->kind == SCOPE_LAMBDA) return true;
        }
        return false;
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

void semantic_warning(SemanticAnalyzer* sem, int line, int column,
                      const char* format, ...) {
    if (!sem) return;
    // Warning does NOT set had_error - the compilation can still proceed.
    sem->warning_count++;

    fprintf(stderr, "[semantic warning] line %d, col %d: ", line, column);

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
            break;

      case AST_BREAK:
            if (!is_inside_loop(sem)) {
                semantic_error(sem, node->line, node->column,
                               "'break' outside loop");
            }
            break;

        case AST_CONTINUE:
            if (!is_inside_loop(sem)) {
                semantic_error(sem, node->line, node->column,
                               "'continue' outside loop");
            }
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
            // Arity check: if the callee is a bare identifier resolving to
            // a function/method with known param_count, compare against
            // actual argument count. Silently skip other callees (subscripts,
            // attribute access, variables holding functions) - we can't
            // reliably determine their arity today.
            if (node->as.call.callee &&
                node->as.call.callee->type == AST_IDENTIFIER) {
                Symbol* fsym = symbol_lookup(sem,
                    node->as.call.callee->as.identifier.name);
                if (fsym &&
                    (fsym->kind == SYM_FUNCTION || fsym->kind == SYM_METHOD) &&
                    fsym->param_count >= 0 &&
                    fsym->param_count != node->as.call.arg_count) {
                    semantic_error(sem, node->line, node->column,
                        "'%s' expects %d argument(s) but got %d",
                        node->as.call.callee->as.identifier.name,
                        fsym->param_count,
                        node->as.call.arg_count);
                }
            }

            break;
        case AST_SUBSCRIPT:
            analyze_node(sem, node->as.subscript.object);
            analyze_node(sem, node->as.subscript.index);
            break;
        case AST_ATTRIBUTE:
            analyze_node(sem, node->as.attribute.object);
            break;

      case AST_LIST_LITERAL:
          // Walk each element - they get name resolution etc.
          for (int i = 0; i < node->as.list_literal.count; i++) {
              analyze_node(sem, node->as.list_literal.elements[i]);
          }
          break;
      case AST_DICT_LITERAL:
          // Walk keys and values.
          for (int i = 0; i < node->as.dict_literal.count; i++) {
              analyze_node(sem, node->as.dict_literal.entries[i].key);
              analyze_node(sem, node->as.dict_literal.entries[i].value);
          }
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
                Symbol* asym = symbol_define(sem,
                node->as.assignment.target->as.identifier.name,
                SYM_VARIABLE,
                node->line, node->column);
          if (asym) {
              // Infer type from RHS literal if possible; otherwise ANY.
              Type* rhs_type = type_of_literal(node->as.assignment.value);
              asym->type = rhs_type ? rhs_type : type_create_primitive(TYPE_ANY);
          }
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
            if (!is_inside_function(sem)) {
                semantic_error(sem, node->line, node->column,
                               "'return' outside function");
            }
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
            scope_push(sem, SCOPE_LOOP_BODY);
            analyze_block_body(sem, node->as.while_stmt.body);
            scope_pop(sem);
            break;

        case AST_FOR:
            analyze_node(sem, node->as.for_stmt.iterable);
            scope_push(sem, SCOPE_LOOP_BODY);
            // Loop variable is defined in the loop's block scope.
            if (node->as.for_stmt.var_name) {
              Symbol* fsym = symbol_define(sem, node->as.for_stmt.var_name,
                          SYM_VARIABLE, node->line, node->column);
              if (fsym) fsym->type = type_create_primitive(TYPE_ANY);
            }
            analyze_block_body(sem, node->as.for_stmt.body);
            scope_pop(sem);
            break;

        case AST_WITH:
            analyze_node(sem, node->as.with_stmt.context);
            scope_push(sem, SCOPE_BLOCK);
            // 'as' binding (if present) is defined in the with-block scope.
            if (node->as.with_stmt.var_name) {
              Symbol* wsym = symbol_define(sem, node->as.with_stmt.var_name,
                          SYM_VARIABLE, node->line, node->column);
              if (wsym) wsym->type = type_create_primitive(TYPE_ANY);
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
                Symbol* fsym = symbol_define(sem, node->as.function_def.name,
                                             fk, node->line, node->column);
                if (fsym) {
                    // Record arity so call sites can validate argument counts.
                    fsym->param_count = node->as.function_def.param_count;
                    // Build TYPE_FUNCTION from annotations.
                    int pc = node->as.function_def.param_count;
                    Type** ptypes = pc > 0
                        ? (Type**)malloc(sizeof(Type*) * pc)
                        : NULL;
                    for (int i = 0; i < pc; i++) {
                        ptypes[i] = type_from_annotation(
                            node->as.function_def.params[i].type_annotation);
                    }
                    Type* ret = type_from_annotation(
                        node->as.function_def.return_type);
                    fsym->type = type_create_function(ptypes, pc, ret);
                }
            }
            scope_push(sem, SCOPE_FUNCTION);
            // Parameters live in the function's own scope.
            for (int i = 0; i < node->as.function_def.param_count; i++) {
                Symbol* psym = symbol_define(sem, node->as.function_def.params[i].name,
                              SYM_PARAMETER, node->line, node->column);
                if (psym) {
                    psym->type = type_from_annotation(
                        node->as.function_def.params[i].type_annotation);
                }
            }
            analyze_block_body(sem, node->as.function_def.body);
            scope_pop(sem);
            break;

      case AST_LAMBDA:
        scope_push(sem, SCOPE_LAMBDA);
        // Lambda parameters live in the lambda's own scope.
        for (int i = 0; i < node->as.lambda.param_count; i++) {
            Symbol* lsym = symbol_define(sem, node->as.lambda.param_names[i],
                          SYM_PARAMETER, node->line, node->column);
            if (lsym) lsym->type = type_create_primitive(TYPE_ANY);
        }
        analyze_node(sem, node->as.lambda.body);
        scope_pop(sem);
        break;

      case AST_TERNARY:
        // No new scope - all three branches live in the current scope.
        analyze_node(sem, node->as.ternary.condition);
        analyze_node(sem, node->as.ternary.then_expr);
        analyze_node(sem, node->as.ternary.else_expr);
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
