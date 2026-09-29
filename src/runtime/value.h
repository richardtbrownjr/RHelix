// value.h - Runtime value representation for RHelix
//
// A Value is the runtime "currency" - every literal produces a Value, every
// expression evaluates to a Value, every variable stores a Value.
//
// Values are held by-value in most contexts (small structs, cheap to copy),
// but any owned heap data (like a string's chars, or a list's elements) must
// be freed via value_destroy when the Value goes out of scope.
//
// Ownership model:
// - Primitive values (NONE, BOOL, INT, FLOAT) own nothing on the heap
// - VAL_STRING owns its char* buffer (freed by value_destroy)
// - Compound values (LIST, DICT, etc.) not yet in scope for Session 1
//
// Copying: value_clone produces a deep copy. Direct struct assignment
// (`Value b = a;`) is a shallow copy - safe for primitives, dangerous for
// strings (both would share the same buffer, double-free hazard).

#ifndef RHELIX_VALUE_H
#define RHELIX_VALUE_H

#include <stdbool.h>

typedef enum {
    VAL_NONE,      // Python's None
    VAL_BOOL,      // true or false
    VAL_INT,       // 64-bit signed integer
    VAL_FLOAT,     // 64-bit double
    VAL_STRING,    // Heap-allocated char buffer + length
    VAL_FUNCTION,  // Callable - AST + captured environment
    VAL_NATIVE,    // C-implemented function callable from RHelix
} ValueKind;

// Forward declarations to avoid circular includes.
// The actual types live in ast.h and environment.h.
struct ASTNode;
struct Environment;
typedef struct Value Value;

// A callable value: function AST + the environment where it was
// defined (the closure). Multiple Values may reference the same
// FunctionValue - lifetime is currently "immortal" (leaked at
// program end). Refcounting or GC will fix that in a future session.
typedef struct FunctionValue {
    struct ASTNode* definition;    // AST_FUNCTION_DEF node - non-owning
    struct Environment* closure;   // Captured env - non-owning
    char* name;                    // Owned - strdup'd
} FunctionValue;

// A native (C-implemented) function callable from RHelix code. Used
// for built-ins like print, len, range. The C function receives an
// arg count and an array of Values, returns a single Value. Arity
// of -1 means variadic (any argc accepted); otherwise argc must
// match arity exactly.
//
// Same "immortal" ownership rule as FunctionValue: allocated once at
// startup by native_fns_install, never destroyed until process exit.
typedef struct NativeFunction {
    char* name;                                    // Owned - strdup'd
    Value (*fn)(int argc, Value* argv);            // C function pointer
    int arity;                                     // -1 for variadic
} NativeFunction;

typedef struct Value {
    ValueKind kind;
    union {
        bool b;
        long long i;
        double f;
        struct {
            char* chars;   // Owned; freed by value_destroy
            int length;    // Byte length, not including null terminator
        } string;
        FunctionValue* function;  // Non-owning pointer (see FunctionValue notes)
        NativeFunction* native;   // Non-owning pointer (immortal)
    } as;
} Value;

// === Constructors ===
// Each returns a fresh Value the caller owns. For VAL_STRING, the input
// C-string is copied into a freshly-allocated buffer.

Value value_none(void);
Value value_bool(bool b);
Value value_int(long long i);
Value value_float(double f);
Value value_string(const char* chars);  // Copies input
Value value_function(FunctionValue* fn);
Value value_native(NativeFunction* fn);

// === Destructor ===
// Frees any heap data owned by the Value. Safe to call on primitives
// (they have nothing to free). Safe to call multiple times only if you
// zero-out kind between calls.

void value_destroy(Value* value);

// === Debug ===
// Returns a heap-allocated string representation, caller frees.
// Examples: "None", "true", "42", "3.14", "\"hello\""

char* value_to_string(Value value);

// Like value_to_string but strings are unquoted (for user-facing output
// like print() rather than debug output). Caller must free.
char* value_to_display(Value value);

// === Equality ===
// Structural equality: same kind, same payload. Strings compared by content.

bool value_equals(Value a, Value b);

// === Clone ===
// Returns a deep copy of a Value. For primitives this is a struct
// copy. For VAL_STRING this duplicates the char buffer so the clone
// and original have independent ownership. Used when we need to
// return a Value the caller will own, but the source is owned by
// somewhere else (e.g., env_get returns a pointer into env storage;
// we clone before returning to the evaluator's caller).

Value value_clone(Value source);

#endif // RHELIX_VALUE_H
