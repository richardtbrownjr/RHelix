// environment.c - Runtime environment (variable scope) for RHelix

#include "environment.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Initial capacity for the bindings array. Grows as needed.
#define INITIAL_CAPACITY 8

// === Lifecycle ===

Environment* env_create(Environment* parent) {
    Environment* env = (Environment*)malloc(sizeof(Environment));
    if (!env) return NULL;

    env->bindings = (Binding*)malloc(sizeof(Binding) * INITIAL_CAPACITY);
    if (!env->bindings) {
        free(env);
        return NULL;
    }

    env->count = 0;
    env->capacity = INITIAL_CAPACITY;
    env->parent = parent;
    return env;
}

void env_destroy(Environment* env) {
    if (!env) return;

    // Free each binding's name string and destroy its Value.
    for (int i = 0; i < env->count; i++) {
        free(env->bindings[i].name);
        value_destroy(&env->bindings[i].value);
    }

    free(env->bindings);
    free(env);
}

// === Helpers ===

// Find a binding by name in THIS scope only (does not walk parents).
// Returns the index, or -1 if not found.
static int find_local(Environment* env, const char* name) {
    for (int i = 0; i < env->count; i++) {
        if (strcmp(env->bindings[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

// Grow the bindings array when we hit capacity.
static bool grow_if_needed(Environment* env) {
    if (env->count < env->capacity) return true;

    int new_capacity = env->capacity * 2;
    Binding* new_bindings = (Binding*)realloc(
        env->bindings, sizeof(Binding) * new_capacity);
    if (!new_bindings) return false;

    env->bindings = new_bindings;
    env->capacity = new_capacity;
    return true;
}

// === Operations ===

void env_define(Environment* env, const char* name, Value value) {
    if (!env || !name) {
        value_destroy(&value);  // Would leak otherwise
        return;
    }

    // If name already exists in THIS scope, overwrite the Value.
    int existing = find_local(env, name);
    if (existing >= 0) {
        value_destroy(&env->bindings[existing].value);
        env->bindings[existing].value = value;
        return;
    }

    // New binding. Grow storage if needed.
    if (!grow_if_needed(env)) {
        value_destroy(&value);
        return;
    }

    env->bindings[env->count].name = strdup(name);
    env->bindings[env->count].value = value;
    env->count++;
}

Value* env_get(Environment* env, const char* name) {
    if (!env || !name) return NULL;

    // Walk the scope chain, current up to global.
    for (Environment* current = env; current; current = current->parent) {
        int idx = find_local(current, name);
        if (idx >= 0) {
            return &current->bindings[idx].value;
        }
    }
    return NULL;
}

void env_assign(Environment* env, const char* name, Value value) {
    if (!env || !name) {
        value_destroy(&value);
        return;
    }

    // Walk the scope chain looking for an existing binding.
    for (Environment* current = env; current; current = current->parent) {
        int idx = find_local(current, name);
        if (idx >= 0) {
            value_destroy(&current->bindings[idx].value);
            current->bindings[idx].value = value;
            return;
        }
    }

    // Not found anywhere - define in current scope.
    // (Python's default; can be refined when we add `global`/`nonlocal`.)
    env_define(env, name, value);
}
