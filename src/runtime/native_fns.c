// native_fns.c - Native function implementations

#include "native_fns.h"
#include "value.h"
#include "environment.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// === print(*args) ===
// Prints each arg separated by a space, followed by a newline.
// Uses value_to_display so strings are unquoted (Python-like).
static Value native_print(int argc, Value* argv) {
    for (int i = 0; i < argc; i++) {
        if (i > 0) putchar(' ');
        char* s = value_to_display(argv[i]);
        fputs(s, stdout);
        free(s);
    }
    putchar('\n');
    return value_none();
}

// === len(x) ===
// Returns the length of a string. Lists/dicts coming later.
static Value native_len(int argc, Value* argv) {
    (void)argc;  // Arity checked by eval_call
    Value v = argv[0];
    if (v.kind == VAL_STRING) {
        return value_int((long long)v.as.string.length);
    }
    fprintf(stderr, "[runtime] len() unsupported for this type\n");
    return value_none();
}

// === str(x) ===
// Converts any Value to its display string.
static Value native_str(int argc, Value* argv) {
    (void)argc;
    char* s = value_to_display(argv[0]);
    Value result = value_string(s);
    free(s);
    return result;
}

// === int(x) ===
// Converts to VAL_INT. Supports int/float/string/bool.
static Value native_int(int argc, Value* argv) {
    (void)argc;
    Value v = argv[0];
    switch (v.kind) {
        case VAL_INT:   return value_int(v.as.i);
        case VAL_FLOAT: return value_int((long long)v.as.f);
        case VAL_BOOL:  return value_int(v.as.b ? 1 : 0);
        case VAL_STRING: {
            if (!v.as.string.chars) {
                fprintf(stderr, "[runtime] int() cannot parse empty string\n");
                return value_none();
            }
            char* end;
            long long result = strtoll(v.as.string.chars, &end, 10);
            if (*end != '\0' || end == v.as.string.chars) {
                fprintf(stderr, "[runtime] int() invalid literal: '%s'\n",
                        v.as.string.chars);
                return value_none();
            }
            return value_int(result);
        }
        default:
            fprintf(stderr, "[runtime] int() cannot convert this type\n");
            return value_none();
    }
}

// === float(x) ===
// Converts to VAL_FLOAT. Supports int/float/string/bool.
static Value native_float(int argc, Value* argv) {
    (void)argc;
    Value v = argv[0];
    switch (v.kind) {
        case VAL_INT:   return value_float((double)v.as.i);
        case VAL_FLOAT: return value_float(v.as.f);
        case VAL_BOOL:  return value_float(v.as.b ? 1.0 : 0.0);
        case VAL_STRING: {
            if (!v.as.string.chars) {
                fprintf(stderr, "[runtime] float() cannot parse empty string\n");
                return value_none();
            }
            char* end;
            double result = strtod(v.as.string.chars, &end);
            if (*end != '\0' || end == v.as.string.chars) {
                fprintf(stderr, "[runtime] float() invalid literal: '%s'\n",
                        v.as.string.chars);
                return value_none();
            }
            return value_float(result);
        }
        default:
            fprintf(stderr, "[runtime] float() cannot convert this type\n");
            return value_none();
    }
}

// === bool(x) ===
// Converts to VAL_BOOL via truthiness semantics.
// Rather than duplicate is_truthy from the evaluator, implement
// the same logic here directly.
static Value native_bool(int argc, Value* argv) {
    (void)argc;
    Value v = argv[0];
    switch (v.kind) {
        case VAL_NONE:     return value_bool(false);
        case VAL_BOOL:     return value_bool(v.as.b);
        case VAL_INT:      return value_bool(v.as.i != 0);
        case VAL_FLOAT:    return value_bool(v.as.f != 0.0);
        case VAL_STRING:   return value_bool(v.as.string.length > 0);
        case VAL_FUNCTION: return value_bool(v.as.function != NULL);
        case VAL_NATIVE:   return value_bool(v.as.native != NULL);
        default:           return value_bool(true);
    }
}

// === type(x) ===
// Returns a string naming the Value's type.
static Value native_type(int argc, Value* argv) {
    (void)argc;
    const char* name;
    switch (argv[0].kind) {
        case VAL_NONE:     name = "none"; break;
        case VAL_BOOL:     name = "bool"; break;
        case VAL_INT:      name = "int"; break;
        case VAL_FLOAT:    name = "float"; break;
        case VAL_STRING:   name = "string"; break;
        case VAL_FUNCTION: name = "function"; break;
        case VAL_NATIVE:   name = "native"; break;
        default:           name = "unknown"; break;
    }
    return value_string(name);
}

// === abs(x) ===
// Absolute value. Numeric only.
static Value native_abs(int argc, Value* argv) {
    (void)argc;
    Value v = argv[0];
    if (v.kind == VAL_INT) {
        long long i = v.as.i;
        return value_int(i < 0 ? -i : i);
    }
    if (v.kind == VAL_FLOAT) {
        double f = v.as.f;
        return value_float(f < 0 ? -f : f);
    }
    fprintf(stderr, "[runtime] abs() requires numeric argument\n");
    return value_none();
}

// === Helper: compare two numeric Values for max/min ===
// Returns negative if a < b, positive if a > b, 0 if equal.
// Returns 0 for non-numeric pairs (callers should error separately).
static double numeric_compare(Value a, Value b) {
    double av = (a.kind == VAL_INT) ? (double)a.as.i : a.as.f;
    double bv = (b.kind == VAL_INT) ? (double)b.as.i : b.as.f;
    return av - bv;
}

static bool is_numeric_kind(Value v) {
    return v.kind == VAL_INT || v.kind == VAL_FLOAT;
}

// === max(*args) ===
// Returns largest of numeric arguments. Variadic, requires >=1 arg.
static Value native_max(int argc, Value* argv) {
    if (argc < 1) {
        fprintf(stderr, "[runtime] max() requires at least one argument\n");
        return value_none();
    }
    Value best = argv[0];
    if (!is_numeric_kind(best)) {
        fprintf(stderr, "[runtime] max() requires numeric arguments\n");
        return value_none();
    }
    for (int i = 1; i < argc; i++) {
        if (!is_numeric_kind(argv[i])) {
            fprintf(stderr, "[runtime] max() requires numeric arguments\n");
            return value_none();
        }
        if (numeric_compare(argv[i], best) > 0) {
            best = argv[i];
        }
    }
    return value_clone(best);
}

// === min(*args) ===
// Returns smallest of numeric arguments. Mirror of max.
static Value native_min(int argc, Value* argv) {
    if (argc < 1) {
        fprintf(stderr, "[runtime] min() requires at least one argument\n");
        return value_none();
    }
    Value best = argv[0];
    if (!is_numeric_kind(best)) {
        fprintf(stderr, "[runtime] min() requires numeric arguments\n");
        return value_none();
    }
    for (int i = 1; i < argc; i++) {
        if (!is_numeric_kind(argv[i])) {
            fprintf(stderr, "[runtime] min() requires numeric arguments\n");
            return value_none();
        }
        if (numeric_compare(argv[i], best) < 0) {
            best = argv[i];
        }
    }
    return value_clone(best);
}

// === Registration table ===
// Static NativeFunction records - immortal, shared by all callers.

static NativeFunction print_native = {
    .name = "print",
    .fn = native_print,
    .arity = -1,   // Variadic
};

static NativeFunction len_native = {
    .name = "len",
    .fn = native_len,
    .arity = 1,
};

static NativeFunction str_native = {
    .name = "str",
    .fn = native_str,
    .arity = 1,
};

static NativeFunction int_native = {
    .name = "int",
    .fn = native_int,
    .arity = 1,
};

static NativeFunction float_native = {
    .name = "float",
    .fn = native_float,
    .arity = 1,
};

static NativeFunction bool_native = {
    .name = "bool",
    .fn = native_bool,
    .arity = 1,
};

static NativeFunction type_native = {
    .name = "type",
    .fn = native_type,
    .arity = 1,
};

static NativeFunction abs_native = {
    .name = "abs",
    .fn = native_abs,
    .arity = 1,
};

static NativeFunction max_native = {
    .name = "max",
    .fn = native_max,
    .arity = -1,   // Variadic (>=1)
};

static NativeFunction min_native = {
    .name = "min",
    .fn = native_min,
    .arity = -1,
};
// === Install into global environment ===

void native_fns_install(Environment* global) {
    if (!global) return;
    env_define(global, "print", value_native(&print_native));
    env_define(global, "len", value_native(&len_native));
    env_define(global, "str", value_native(&str_native));
    env_define(global, "int", value_native(&int_native));
    env_define(global, "float", value_native(&float_native));
    env_define(global, "bool", value_native(&bool_native));
    env_define(global, "type", value_native(&type_native));
    env_define(global, "abs", value_native(&abs_native));
    env_define(global, "max", value_native(&max_native));
    env_define(global, "min", value_native(&min_native));
}
