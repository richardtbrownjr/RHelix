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
          char* type_str = type_to_string(s->type);
          printf("    %-10s %-15s [%s] (line %d)\n",
               symbol_kind_to_string(s->kind), s->name,
               type_str, s->defined_line);
          free(type_str);
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
    sym->type_declared = false;  // Walker sets true when populating from annotation

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

// === Type inference ===

// Helper: emit a type error at the given node's location. Returns TYPE_ANY
// so the caller can chain "return type_error_here(...)".
static Type* type_error_here(SemanticAnalyzer* sem, ASTNode* node,
                             const char* format, ...) {
    if (sem && node) {
        sem->had_error = true;
        sem->error_count++;
        fprintf(stderr, "[semantic] line %d, col %d: ",
                node->line, node->column);
        va_list args;
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
        fprintf(stderr, "\n");
    }
    return type_create_primitive(TYPE_ANY);
}

// Helper: infer type from binary arithmetic (+ - * / %). Handles numeric
// promotion (int + float -> float) and string concatenation (+ only).
static Type* infer_binary_arithmetic(SemanticAnalyzer* sem, ASTNode* node,
                                     Type* left, Type* right) {
    TokenType op = node->as.binary.op;

    // Any operand is ANY - defer decision.
    if (left->kind == TYPE_ANY || right->kind == TYPE_ANY) {
        return type_create_primitive(TYPE_ANY);
    }

    // String concatenation: only + supports it.
    if (left->kind == TYPE_STRING && right->kind == TYPE_STRING) {
        if (op == TOKEN_PLUS) return type_create_primitive(TYPE_STRING);
        return type_error_here(sem, node,
            "unsupported string operation '%s'", token_type_to_string(op));
    }

    // Numeric arithmetic with promotion.
    bool l_num = (left->kind == TYPE_INT || left->kind == TYPE_FLOAT);
    bool r_num = (right->kind == TYPE_INT || right->kind == TYPE_FLOAT);
    if (l_num && r_num) {
        // int + int -> int; anything with float -> float
        if (left->kind == TYPE_FLOAT || right->kind == TYPE_FLOAT) {
            return type_create_primitive(TYPE_FLOAT);
        }
        return type_create_primitive(TYPE_INT);
    }

    // Mismatch: emit error and return ANY to keep analysis going.
    char* ls = type_to_string(left);
    char* rs = type_to_string(right);
    Type* err = type_error_here(sem, node,
        "cannot apply '%s' to '%s' and '%s'",
        token_type_to_string(op), ls, rs);
    free(ls);
    free(rs);
    return err;
}

Type* type_of_expression(SemanticAnalyzer* sem, ASTNode* node) {
    if (!node) return type_create_primitive(TYPE_ANY);

    switch (node->type) {
        // === Literals delegate to type_of_literal ===
        case AST_LITERAL_INT:
        case AST_LITERAL_FLOAT:
        case AST_LITERAL_STRING:
        case AST_LITERAL_BOOL:
        case AST_LITERAL_NONE: {
            Type* t = type_of_literal(node);
            return t ? t : type_create_primitive(TYPE_ANY);
        }

        // === Identifier: look up symbol, clone its type ===
        case AST_IDENTIFIER: {
            Symbol* s = symbol_lookup(sem, node->as.identifier.name);
            if (s && s->type) return type_clone(s->type);
            return type_create_primitive(TYPE_ANY);
        }

        // === Grouping: recurse ===
        case AST_GROUPING:
            return type_of_expression(sem, node->as.grouping.expression);

        // === Unary ===
        case AST_UNARY: {
            TokenType op = node->as.unary.op;
            Type* operand = type_of_expression(sem, node->as.unary.operand);
            // 'not' always yields bool.
            if (op == TOKEN_NOT) {
                type_destroy(operand);
                return type_create_primitive(TYPE_BOOL);
            }
            // Unary +/-: valid on int/float only.
            if (op == TOKEN_MINUS || op == TOKEN_PLUS) {
                if (operand->kind == TYPE_ANY) return operand;
                if (operand->kind == TYPE_INT || operand->kind == TYPE_FLOAT) {
                    return operand;
                }
                char* os = type_to_string(operand);
                Type* err = type_error_here(sem, node,
                    "unary '%s' not supported for type '%s'",
                    token_type_to_string(op), os);
                free(os);
                type_destroy(operand);
                return err;
            }
            type_destroy(operand);
            return type_create_primitive(TYPE_ANY);
        }

        // === Binary ===
        case AST_BINARY: {
            TokenType op = node->as.binary.op;
            Type* left = type_of_expression(sem, node->as.binary.left);
            Type* right = type_of_expression(sem, node->as.binary.right);

            // Comparisons always yield bool (regardless of operand types).
            if (op == TOKEN_LESS || op == TOKEN_GREATER ||
                op == TOKEN_LESS_EQUALS || op == TOKEN_GREATER_EQUALS ||
                op == TOKEN_EQUALS_EQUALS || op == TOKEN_NOT_EQUALS) {
                type_destroy(left);
                type_destroy(right);
                return type_create_primitive(TYPE_BOOL);
            }
            // Logical, membership, identity all yield bool.
            if (op == TOKEN_AND || op == TOKEN_OR ||
                op == TOKEN_IN || op == TOKEN_IS) {
                type_destroy(left);
                type_destroy(right);
                return type_create_primitive(TYPE_BOOL);
            }
            // Pipeline: type is the return type of the callee (right side).
            if (op == TOKEN_PIPELINE) {
                Type* result = (right->kind == TYPE_FUNCTION && right->return_type)
                    ? type_clone(right->return_type)
                    : type_create_primitive(TYPE_ANY);
                type_destroy(left);
                type_destroy(right);
                return result;
            }
            // Arithmetic: delegate to helper.
            Type* result = infer_binary_arithmetic(sem, node, left, right);
            type_destroy(left);
            type_destroy(right);
            return result;
        }

        // === Function call ===
        case AST_CALL: {
            // If callee resolves to a function/method with known return type,
            // that's our type. Otherwise ANY.
            ASTNode* callee = node->as.call.callee;
            if (callee && callee->type == AST_IDENTIFIER) {
                Symbol* s = symbol_lookup(sem, callee->as.identifier.name);
                if (s && s->type && s->type->kind == TYPE_FUNCTION &&
                    s->type->return_type) {
                    return type_clone(s->type->return_type);
                }
            }
            return type_create_primitive(TYPE_ANY);
        }

        // === Subscript ===
        case AST_SUBSCRIPT: {
            Type* obj = type_of_expression(sem, node->as.subscript.object);
            Type* result;
            if (obj->kind == TYPE_LIST && obj->element_type) {
                result = type_clone(obj->element_type);
            } else if (obj->kind == TYPE_DICT && obj->element_type) {
                result = type_clone(obj->element_type);
            } else {
                result = type_create_primitive(TYPE_ANY);
            }
            type_destroy(obj);
            return result;
        }

        // === Attribute ===
        case AST_ATTRIBUTE:
            // We don't track instance attributes yet - defer to ANY.
            return type_create_primitive(TYPE_ANY);

        // === Ternary: same-type else ANY ===
        case AST_TERNARY: {
            Type* then_t = type_of_expression(sem, node->as.ternary.then_expr);
            Type* else_t = type_of_expression(sem, node->as.ternary.else_expr);
            // We don't type the condition - it's expected to be truthy-ish.
            Type* result = type_equals(then_t, else_t)
                ? type_clone(then_t)
                : type_create_primitive(TYPE_ANY);
            type_destroy(then_t);
            type_destroy(else_t);
            return result;
        }

        // === Collection literals ===
        case AST_LIST_LITERAL: {
            int count = node->as.list_literal.count;
            if (count == 0) {
                return type_create_list(type_create_primitive(TYPE_ANY));
            }
            Type* first = type_of_expression(sem,
                node->as.list_literal.elements[0]);
            bool consistent = true;
            for (int i = 1; i < count; i++) {
                Type* t = type_of_expression(sem,
                    node->as.list_literal.elements[i]);
                if (!type_equals(first, t)) consistent = false;
                type_destroy(t);
            }
            if (consistent) return type_create_list(first);
            type_destroy(first);
            return type_create_list(type_create_primitive(TYPE_ANY));
        }
        case AST_DICT_LITERAL: {
            int count = node->as.dict_literal.count;
            if (count == 0) {
                return type_create_dict(
                    type_create_primitive(TYPE_ANY),
                    type_create_primitive(TYPE_ANY));
            }
            Type* first_k = type_of_expression(sem,
                node->as.dict_literal.entries[0].key);
            Type* first_v = type_of_expression(sem,
                node->as.dict_literal.entries[0].value);
            bool k_consistent = true, v_consistent = true;
            for (int i = 1; i < count; i++) {
                Type* k = type_of_expression(sem,
                    node->as.dict_literal.entries[i].key);
                Type* v = type_of_expression(sem,
                    node->as.dict_literal.entries[i].value);
                if (!type_equals(first_k, k)) k_consistent = false;
                if (!type_equals(first_v, v)) v_consistent = false;
                type_destroy(k);
                type_destroy(v);
            }
            Type* result_k = k_consistent ? first_k : type_create_primitive(TYPE_ANY);
            Type* result_v = v_consistent ? first_v : type_create_primitive(TYPE_ANY);
            if (!k_consistent) type_destroy(first_k);
            if (!v_consistent) type_destroy(first_v);
            return type_create_dict(result_k, result_v);
        }

        // === Lambda: TYPE_FUNCTION with ANY params, inferred body return ===
        case AST_LAMBDA: {
            int pc = node->as.lambda.param_count;
            Type** ptypes = pc > 0
                ? (Type**)malloc(sizeof(Type*) * pc)
                : NULL;
            for (int i = 0; i < pc; i++) {
                ptypes[i] = type_create_primitive(TYPE_ANY);
            }
            Type* ret = type_of_expression(sem, node->as.lambda.body);
            return type_create_function(ptypes, pc, ret);
        }

        default:
            return type_create_primitive(TYPE_ANY);
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
      case AST_ASSIGNMENT: {
      // Assignment to a plain identifier is either a definition
      // (first use in this scope) or a rebind (name already exists).
      // Attribute and subscript targets don't define new names.
      if (node->as.assignment.target &&
          node->as.assignment.target->type == AST_IDENTIFIER) {

          const char* name = node->as.assignment.target->as.identifier.name;
          Symbol* existing = symbol_lookup_local(sem, name);
          Type* rhs_type = type_of_expression(sem, node->as.assignment.value);

          if (existing) {
              // Rebind. If existing has a declared type (non-ANY),
              // check that the new value is compatible. ANY on
              // either side is treated as wildcard.
              if (existing->type &&
                  existing->type_declared &&
                  existing->type->kind != TYPE_ANY &&
                  rhs_type->kind != TYPE_ANY &&
                  !type_equals(existing->type, rhs_type)) {
                  char* lhs_str = type_to_string(existing->type);
                  char* rhs_str = type_to_string(rhs_type);
                  semantic_error(sem, node->line, node->column,
                      "cannot assign '%s' to '%s' of type '%s'",
                      rhs_str, name, lhs_str);
                  free(lhs_str);
                  free(rhs_str);
              }
              // Rebind does NOT change the symbol's declared type.
              type_destroy(rhs_type);
          } else {
              // First use: define and set the inferred type.
              Symbol* asym = symbol_define(sem, name, SYM_VARIABLE,
                                           node->line, node->column);
              if (asym) {
                  asym->type = rhs_type;
              } else {
                  type_destroy(rhs_type);
              }
          }
      } else {
          // Non-identifier target: still walk value for name resolution.
          analyze_node(sem, node->as.assignment.value);
      }
      analyze_node(sem, node->as.assignment.target);
      // Note: for the identifier case we already walked value implicitly
      // via type_of_expression, but a redundant walk is safe (name
      // resolution is idempotent) and keeps the code simple.
      if (node->as.assignment.target &&
          node->as.assignment.target->type == AST_IDENTIFIER) {
          analyze_node(sem, node->as.assignment.value);
      }
      break;
    }
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
                // Infer the return value's type. For today this only serves
                // to fire arithmetic errors inside the return expression;
                // Session 4 will compare against the function's declared
                // return type.
                Type* rt = type_of_expression(sem, node->as.ret.value);
                type_destroy(rt);
            }
            break;

          case AST_ASSERT:
              // Assert doesn't create scope. Walk condition (triggers name
              // resolution and arithmetic type checks inside it) and message
              // if present. Runtime semantics (raise AssertionError when
              // condition is false) are code-gen's problem.
              analyze_node(sem, node->as.assert_stmt.condition);
              if (node->as.assert_stmt.message) {
                analyze_node(sem, node->as.assert_stmt.message);
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
                    fsym->type_declared = true;
                }
            }
            scope_push(sem, SCOPE_FUNCTION);
            // Parameters live in the function's own scope.
            for (int i = 0; i < node->as.function_def.param_count; i++) {
                Symbol* psym = symbol_define(sem, node->as.function_def.params[i].name,
                              SYM_PARAMETER, node->line, node->column);
                  if (psym) {
                    ASTNode* ann = node->as.function_def.params[i].type_annotation;
                    psym->type = type_from_annotation(ann);
                    psym->type_declared = (ann != NULL);
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
              Symbol* csym = symbol_define(sem, node->as.class_def.name,
                        SYM_CLASS, node->line, node->column);
              if (csym) csym->type = type_create_primitive(TYPE_ANY);
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
