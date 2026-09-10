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

// === Destructor ===

void value_destroy(Value* value) {
    if (!value) return;
    switch (value->kind) {
        case VAL_STRING:
            free(value->as.string.chars);
            value->as.string.chars = NULL;
            value->as.string.length = 0;
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
        default:
            return false;
    }
}
