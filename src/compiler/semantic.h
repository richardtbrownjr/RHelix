#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ast.h"
#include <stdbool.h>

// ScopeKind classifies why a new scope exists. This matters because different
// scope kinds have different rules downstream - e.g., return statements are
// only valid inside FUNCTION scopes, break/continue only inside loops, and
// class scopes affect name resolution differently than function scopes.
typedef enum {
    SCOPE_MODULE,    // Top-level module scope (outermost)
    SCOPE_FUNCTION,  // Inside a def
    SCOPE_CLASS,     // Inside a class body
    SCOPE_BLOCK,     // Inside an if/while/for/with body
    SCOPE_LAMBDA     // Inside a lambda body (function-like but expression-context)
} ScopeKind;

// Scope is a node in a stack (linked-list). Each scope points to its parent,
// so we can walk outward from the current scope to find enclosing contexts.
// The head of the stack is the "innermost" scope; the tail is SCOPE_MODULE.
typedef struct Scope {
    ScopeKind kind;
    struct Scope* parent;  // NULL only for the module scope
    int depth;             // 0 for module, increments with each push
} Scope;

// SemanticAnalyzer holds all state that persists across the entire analysis
// of one module. Right now that's just the current scope pointer and an
// error flag. As we add features (symbol tables, type environments, error
// lists), they land in this struct.
typedef struct {
    Scope* current_scope;
    bool had_error;
    // Track deepest scope depth reached during analysis - useful for
    // debugging and validates that push/pop are balanced.
    int max_depth_reached;
} SemanticAnalyzer;

// === Lifecycle ===
SemanticAnalyzer* semantic_create(void);
void semantic_destroy(SemanticAnalyzer* sem);

// === Main entry point ===
// Walks the AST rooted at 'module' (which must be AST_MODULE), maintaining
// scope state. Returns true if analysis completed without errors, false
// if any semantic error was detected. Right now this only validates that
// scope push/pop is balanced - real checks come in future sessions.
bool semantic_analyze(SemanticAnalyzer* sem, ASTNode* module);

// === Internal helpers (exposed for testing) ===
// These are visible so test_semantic.c can inspect scope state during
// analysis. In production these would typically be static-internal.
void scope_push(SemanticAnalyzer* sem, ScopeKind kind);
void scope_pop(SemanticAnalyzer* sem);
Scope* scope_current(SemanticAnalyzer* sem);
const char* scope_kind_to_string(ScopeKind kind);

#endif
