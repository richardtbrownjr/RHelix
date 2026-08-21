#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ast.h"
#include "types.h"
#include <stdbool.h>

// ScopeKind classifies why a new scope exists. This matters because different
// scope kinds have different rules downstream - e.g., return statements are
// only valid inside FUNCTION scopes, break/continue only inside loops, and
// class scopes affect name resolution differently than function scopes.
typedef enum {
    SCOPE_MODULE,    // Top-level module scope (outermost)
    SCOPE_FUNCTION,  // Inside a def
    SCOPE_CLASS,     // Inside a class body
    SCOPE_BLOCK,     // Inside an if/else or with body (non-loop block)
    SCOPE_LOOP_BODY, // Inside a while or for body (needed for break/continue validation)ß
    SCOPE_LAMBDA     // Inside a lambda body (function-like but expression-context)
} ScopeKind;

// Scope is a node in a stack (linked-list). Each scope points to its parent,
// so we can walk outward from the current scope to find enclosing contexts.
// The head of the stack is the "innermost" scope; the tail is SCOPE_MODULE.
// SymbolKind classifies what a name represents in the source code. This
// matters because different kinds have different resolution rules:
// functions can be forward-referenced within a class body but variables
// cannot, parameters are only visible inside their function, etc. Right
// now the analyzer doesn't enforce those rules yet - we just tag symbols
// with their kind so future features can reason about them.
typedef enum {
    SYM_VARIABLE,   // Assignment target at module or function level
    SYM_PARAMETER,  // Function or lambda parameter
    SYM_FUNCTION,   // Def declaration
    SYM_CLASS,      // Class declaration
    SYM_METHOD      // Def inside a class body
} SymbolKind;

// Symbol represents one named entity within a scope. The scope's
// symbol_table is a singly-linked list of these. Order of insertion is
// preserved by prepending to the list (newer symbols shadow older ones
// with the same name during lookup, matching lexical scoping rules).
typedef struct Symbol {
    char* name;              // Owned string (strdup on insert)
    SymbolKind kind;
    int defined_line;        // Where this symbol was defined
    int defined_column;
    struct Symbol* next;     // Next symbol in the same scope's table
    int param_count;         // Number of declared params (SYM_FUNCTION/METHOD only, -1 otherwise)
    Type* type;              // Inferred or declared type of this symbol (owned)
} Symbol;

typedef struct Scope {
    ScopeKind kind;
    struct Scope* parent;  // NULL only for the module scope
    int depth;             // 0 for module, increments with each push
    Symbol* symbol_table;  // Head of the linked list of symbols in this scope
} Scope;

// SemanticAnalyzer holds all state that persists across the entire analysis
// of one module. Right now that's just the current scope pointer and an
// error flag. As we add features (symbol tables, type environments, error
// lists), they land in this struct.
typedef struct {
    Scope* current_scope;
    bool had_error;
    int error_count;   // Number of semantic errors reported during analysis
    int warning_count; // Number of semantic warnings emitted (non-fatal)
    // Track deepest scope depth reached during analysis - useful for
    // debugging and validates that push/pop are balanced.
    int max_depth_reached;
    bool debug_print_scopes;  // If true, scope_pop prints symbol table before freeing
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

// === Symbol operations ===
// Symbols are stored in the current scope's symbol_table. Insert prepends
// to the list so newer definitions shadow older ones with the same name
// during lookup (the "shadowing" that closures depend on).

// Define a new symbol in the current scope. If a symbol with the same
// name already exists in the current scope (not a parent scope), this
// still inserts - shadowing is the caller's decision. Returns the new
// symbol or NULL on allocation failure.
Symbol* symbol_define(SemanticAnalyzer* sem, const char* name, SymbolKind kind,
                      int line, int column);

// Look up a symbol by name. Searches the current scope first, then walks
// up the parent chain to the module scope. Returns NULL if not found.
Symbol* symbol_lookup(SemanticAnalyzer* sem, const char* name);

// Look up a symbol only in the current scope (does not walk parents).
// Useful for detecting redefinitions within a single scope.
Symbol* symbol_lookup_local(SemanticAnalyzer* sem, const char* name);

// Convert SymbolKind to a debug string. Used for test output and error
// messages.
const char* symbol_kind_to_string(SymbolKind kind);

// === Error reporting ===

// Report a semantic error. Prints to stderr with source location, sets
// had_error, and increments error_count. Right now errors are one-shot -
// there is no list to collect them into. Tests can assert on error_count
// but not on specific error messages. Improving this to a list of
// structured errors is a future polish.
void semantic_error(SemanticAnalyzer* sem, int line, int column,
                    const char* format, ...);

// Report a semantic warning (non-fatal). Prints to stderr with source
// location, increments warning_count but does NOT set had_error. Used
// for issues that are technically valid code but likely bugs
// (redeclaration, shadowing, unused variables in the future).
void semantic_warning(SemanticAnalyzer* sem, int line, int column,
                      const char* format, ...);

#endif
