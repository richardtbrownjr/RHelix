// test_value.c - Standalone tests for Value runtime representation

#include "value.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Simple test harness: prints a header, runs a test function, tracks pass/fail.
static int tests_run = 0;
static int tests_failed = 0;

static void check(const char* description, bool condition) {
    tests_run++;
    if (condition) {
        printf("  ✓ %s\n", description);
    } else {
        printf("  ✗ %s\n", description);
        tests_failed++;
    }
}

// Helper: check that value_to_string returns the expected debug string.
static void check_debug_string(const char* description, Value v, const char* expected) {
    tests_run++;
    char* got = value_to_string(v);
    if (got && strcmp(got, expected) == 0) {
        printf("  ✓ %s → %s\n", description, got);
    } else {
        printf("  ✗ %s → expected \"%s\", got \"%s\"\n",
               description, expected, got ? got : "<null>");
        tests_failed++;
    }
    free(got);
}

int main(void) {
    printf("\n========== VALUE REPRESENTATION TESTS ==========\n\n");

    // ---- Constructors and kind tags ----
    printf("Constructors:\n");
    Value n = value_none();
    check("value_none has kind VAL_NONE", n.kind == VAL_NONE);

    Value t = value_bool(true);
    check("value_bool(true) has kind VAL_BOOL",  t.kind == VAL_BOOL);
    check("value_bool(true).as.b is true",       t.as.b == true);

    Value f = value_bool(false);
    check("value_bool(false).as.b is false",     f.as.b == false);

    Value i = value_int(42);
    check("value_int(42) has kind VAL_INT",      i.kind == VAL_INT);
    check("value_int(42).as.i is 42",            i.as.i == 42);

    Value neg = value_int(-1);
    check("value_int(-1).as.i is -1",            neg.as.i == -1);

    Value pi = value_float(3.14);
    check("value_float(3.14) has kind VAL_FLOAT", pi.kind == VAL_FLOAT);
    check("value_float(3.14).as.f is 3.14",       pi.as.f == 3.14);

    Value greet = value_string("hello");
    check("value_string(\"hello\") has kind VAL_STRING",
          greet.kind == VAL_STRING);
    check("value_string(\"hello\").as.string.length is 5",
          greet.as.string.length == 5);
    check("value_string(\"hello\") copies chars",
          greet.as.string.chars != NULL &&
          strcmp(greet.as.string.chars, "hello") == 0);

    // ---- Debug strings ----
    printf("\nDebug strings:\n");
    check_debug_string("None",       n,     "None");
    check_debug_string("bool true",  t,     "true");
    check_debug_string("bool false", f,     "false");
    check_debug_string("int 42",     i,     "42");
    check_debug_string("int -1",     neg,   "-1");
    check_debug_string("float 3.14", pi,    "3.14");
    check_debug_string("string",     greet, "\"hello\"");

    // ---- Equality ----
    printf("\nEquality:\n");
    check("None == None",                 value_equals(value_none(), value_none()));
    check("true == true",                 value_equals(value_bool(true), value_bool(true)));
    check("true != false",                !value_equals(value_bool(true), value_bool(false)));
    check("42 == 42",                     value_equals(value_int(42), value_int(42)));
    check("42 != 43",                     !value_equals(value_int(42), value_int(43)));
    check("3.14 == 3.14",                 value_equals(value_float(3.14), value_float(3.14)));
    check("\"hello\" == \"hello\"",       value_equals(value_string("hello"), value_string("hello")));
    check("\"hello\" != \"world\"",       !value_equals(value_string("hello"), value_string("world")));
    check("int 42 != float 42",           !value_equals(value_int(42), value_float(42.0)));
    check("None != false",                !value_equals(value_none(), value_bool(false)));

    // ---- Destruction ----
    printf("\nDestruction:\n");
    // Destroy primitive: no-op, but should not crash.
    value_destroy(&i);
    check("destroy int does not crash",   true);

    // Destroy string: frees the buffer.
    Value s = value_string("goodbye");
    value_destroy(&s);
    check("destroy string does not crash", true);
    check("destroy nulls out string.chars",  s.as.string.chars == NULL);
    check("destroy zeros string.length",     s.as.string.length == 0);

    // Destroy the earlier string too (avoid leak).
    value_destroy(&greet);

    // ---- Summary ----
    printf("\n========== SUMMARY ==========\n");
    printf("  Tests run:    %d\n", tests_run);
    printf("  Tests passed: %d\n", tests_run - tests_failed);
    printf("  Tests failed: %d\n", tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
