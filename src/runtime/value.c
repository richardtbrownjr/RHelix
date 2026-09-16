// value.c - Runtime value representation for RHelix

#include "value.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// === Constructors ===

Value value_none(void) {
    Value v;
    v.kind = VAL_NONE;
    return v;
}

Value value_bool(bool b) {
    Value v;
    v.kind = VAL_BOOL;
    v.as.b = b;
    return v;
}

Value value_int(long long i) {
    Value v;
    v.kind = VAL_INT;
    v.as.i = i;
    return v;
}

Value value_float(double f) {
    Value v;
    v.kind = VAL_FLOAT;
    v.as.f = f;
    return v;
}

Value value_string(const char* chars) {
    Value v;
    v.kind = VAL_STRING;
    if (chars) {
        v.as.string.length = (int)strlen(chars);
        v.as.string.chars = (char*)malloc(v.as.string.length + 1);
        if (v.as.string.chars) {
            memcpy(v.as.string.chars, chars, v.as.string.length + 1);
        }
    } else {
        v.as.string.chars = NULL;
        v.as.string.length = 0;
    }
    return v;
}
Value value_function(FunctionValue* fn) {
    Value v;
    v.kind = VAL_FUNCTION;
    v.as.function = fn;
    return v;
}

// === Destructor ===

void value_destroy(Value* value) {
    if (!value) return;
    switch (value->kind) {
        case VAL_STRING:
            free(value->as.string.chars);
            value->as.string.chars = NULL;
            value->as.string.length = 0;
            break;
        case VAL_FUNCTION:
            // Function values are non-owning - the FunctionValue lives
            // beyond the Value's lifetime (shared across clones,
            // stored in environment). Destroyed only at process exit
            // for now. Refcounting or GC will fix this later.
            break;
        default:
            // Primitives own nothing on the heap.
            break;
    }
}

// === Debug ===

char* value_to_string(Value value) {
    // Buffer sized generously - snprintf caps at buffer size.
    char buf[128];
    switch (value.kind) {
        case VAL_NONE:
            return strdup("None");
        case VAL_BOOL:
            return strdup(value.as.b ? "true" : "false");
        case VAL_INT:
            snprintf(buf, sizeof(buf), "%lld", value.as.i);
            return strdup(buf);
        case VAL_FLOAT:
            // %g avoids trailing zeros while preserving precision
            snprintf(buf, sizeof(buf), "%g", value.as.f);
            return strdup(buf);
        case VAL_STRING: {
            // Wrap in quotes for debug output. Allocate length + 3 for the
            // two quotes and the null terminator.
            int len = value.as.string.length;
            char* result = (char*)malloc(len + 3);
            if (!result) return strdup("<oom>");
            result[0] = '"';
            if (value.as.string.chars) {
                memcpy(result + 1, value.as.string.chars, len);
            }
            result[len + 1] = '"';
            result[len + 2] = '\0';
            return result;
        }
      case VAL_FUNCTION: {
          char buf[128];
          snprintf(buf, sizeof(buf), "<function %s>",
              value.as.function && value.as.function->name
                  ? value.as.function->name : "?");
          return strdup(buf);
        }
        default:
            return strdup("<unknown>");
    }
}

// === Equality ===

bool value_equals(Value a, Value b) {
    if (a.kind != b.kind) return false;
    switch (a.kind) {
        case VAL_NONE:
            return true;
        case VAL_BOOL:
            return a.as.b == b.as.b;
        case VAL_INT:
            return a.as.i == b.as.i;
        case VAL_FLOAT:
            return a.as.f == b.as.f;
        case VAL_STRING:
            if (a.as.string.length != b.as.string.length) return false;
            if (!a.as.string.chars || !b.as.string.chars) {
                return a.as.string.chars == b.as.string.chars;
            }
            return memcmp(a.as.string.chars, b.as.string.chars,
                          a.as.string.length) == 0;
        case VAL_FUNCTION:
          // Function equality is identity - same FunctionValue pointer
          return a.as.function == b.as.function;
        default:
            return false;
    }
}

// === Clone ===

Value value_clone(Value source) {
    switch (source.kind) {
        case VAL_NONE:
        case VAL_BOOL:
        case VAL_INT:
        case VAL_FLOAT:
        case VAL_FUNCTION:
            // Primitives: struct copy is sufficient, nothing owned.
            return source;
        case VAL_STRING: {
            // Duplicate the char buffer for independent ownership.
            Value copy;
            copy.kind = VAL_STRING;
            copy.as.string.length = source.as.string.length;
            if (source.as.string.chars && source.as.string.length > 0) {
                copy.as.string.chars = (char*)malloc(source.as.string.length + 1);
                if (copy.as.string.chars) {
                    memcpy(copy.as.string.chars,
                           source.as.string.chars,
                           source.as.string.length + 1);
                } else {
                    copy.as.string.length = 0;
                }
            } else {
                copy.as.string.chars = NULL;
            }
            return copy;
        }
        default:
            // Unknown kinds return None (safe fallback).
            return value_none();
    }
}
