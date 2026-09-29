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

// === Registration table ===
// Static NativeFunction records - immortal, shared by all callers.

static NativeFunction print_native = {
    .name = "print",
    .fn = native_print,
    .arity = -1,   // Variadic
};

// === Install into global environment ===

void native_fns_install(Environment* global) {
    if (!global) return;
    env_define(global, "print", value_native(&print_native));
}
