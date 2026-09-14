// environment.h - Runtime environment (variable scope) for RHelix
//
// An Environment maps variable names to Values. Every scope in a
// running program has one - the global scope, each function call,
// each nested block.
//
// Environments chain via parent pointers to form scope chains. When
// looking up a name, we walk the chain from current scope up to
// global. The first match wins (that's how shadowing works).
//
// Ownership contract:
// - Environment OWNS its name strings (strdup'd on define, freed on destroy)
// - Environment OWNS its Value bindings (destroyed on env_destroy)
// - Parent pointer is a non-owning reference - destroying a child
//   scope does NOT destroy the parent
// - env_get returns a NON-OWNING pointer into the env's storage.
//   The caller can READ the Value but must not free it. If the
//   caller needs to keep the Value beyond the environment's
//   lifetime, they must copy it (value_clone will exist eventually)
//
// Storage: linear array with linear search. Environments typically
// have small numbers of bindings (10-50) so a hash map is
// unnecessary overhead. If profiling ever shows this matters, we
// can swap in a hash map behind the same API.

#ifndef RHELIX_ENVIRONMENT_H
#define RHELIX_ENVIRONMENT_H

#include "value.h"

// A single name → value binding.
typedef struct Binding {
    char* name;    // Owned; strdup'd on define
    Value value;   // Owned; destroyed on env_destroy
} Binding;

// An environment (scope). Bindings live in a dynamically-grown array.
// Parent pointer chains scopes for name lookup.
typedef struct Environment {
    Binding* bindings;
    int count;
    int capacity;
    struct Environment* parent;  // NULL for global scope
} Environment;

// === Lifecycle ===

// Create a new environment. Pass NULL for the global scope.
// For a nested scope (function call, block), pass the enclosing env.
Environment* env_create(Environment* parent);

// Destroy an environment. Frees all bindings (name strings and
// Values). Does NOT touch the parent - parents have their own
// lifecycle.
void env_destroy(Environment* env);

// === Operations ===

// Define a new binding in the current scope.
// If the name already exists in THIS scope (not walking parents),
// the old Value is destroyed and replaced. This matches Python's
// overwrite semantics for `x = 5; x = 6`.
void env_define(Environment* env, const char* name, Value value);

// Look up a name, walking the parent chain from current up to global.
// Returns a non-owning pointer to the Value, or NULL if the name
// is not found in any scope.
Value* env_get(Environment* env, const char* name);

// Assign to an existing binding. Walks the parent chain to find
// where the name is defined and updates it there. If not found
// anywhere, defines it in the current scope (Python's default
// assignment behavior without `global`/`nonlocal`).
void env_assign(Environment* env, const char* name, Value value);

#endif // RHELIX_ENVIRONMENT_H
