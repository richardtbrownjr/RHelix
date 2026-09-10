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
} ValueKind;

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

// === Destructor ===
// Frees any heap data owned by the Value. Safe to call on primitives
// (they have nothing to free). Safe to call multiple times only if you
// zero-out kind between calls.

void value_destroy(Value* value);

// === Debug ===
// Returns a heap-allocated string representation, caller frees.
// Examples: "None", "true", "42", "3.14", "\"hello\""

char* value_to_string(Value value);

// === Equality ===
// Structural equality: same kind, same payload. Strings compared by content.

bool value_equals(Value a, Value b);

#endif // RHELIX_VALUE_H
