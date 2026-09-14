// test_environment.c - Tests for runtime Environment
//
// Verifies: define/get in single scope, get returns NULL for missing,
// nested scope lookup walks the parent chain, assignment finds and
// updates enclosing bindings, shadowing (inner scope masks outer),
// child destroy does not affect parent.

#include "environment.h"
#include "value.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int main(void) {
    printf("\n========== ENVIRONMENT TESTS ==========\n\n");

    // ---- Single scope: define and get ----
    printf("Single scope:\n");
    Environment* global = env_create(NULL);
    check("env_create returns non-NULL", global != NULL);
    check("new env has count 0", global->count == 0);
    check("new env has NULL parent", global->parent == NULL);

    env_define(global, "x", value_int(42));
    check("define x = 42 increments count", global->count == 1);

    Value* got = env_get(global, "x");
    check("env_get returns non-NULL for defined name", got != NULL);
    check("env_get returns correct kind", got && got->kind == VAL_INT);
    check("env_get returns correct value", got && got->as.i == 42);

    // ---- Missing name ----
    Value* missing = env_get(global, "nonexistent");
    check("env_get returns NULL for missing name", missing == NULL);

    // ---- Overwrite in same scope ----
    printf("\nOverwrite:\n");
    env_define(global, "x", value_int(100));
    check("re-define same name keeps count at 1", global->count == 1);
    got = env_get(global, "x");
    check("re-define updates value", got && got->as.i == 100);

    // ---- Multiple bindings ----
    printf("\nMultiple bindings:\n");
    env_define(global, "y", value_string("hello"));
    env_define(global, "z", value_bool(true));
    check("three bindings gives count 3", global->count == 3);

    Value* y_val = env_get(global, "y");
    check("get y returns VAL_STRING",
          y_val && y_val->kind == VAL_STRING);
    check("get y returns 'hello'",
          y_val && strcmp(y_val->as.string.chars, "hello") == 0);

    Value* z_val = env_get(global, "z");
    check("get z returns VAL_BOOL", z_val && z_val->kind == VAL_BOOL);
    check("get z returns true", z_val && z_val->as.b == true);

    // ---- Nested scope: parent lookup ----
    printf("\nNested scope (parent lookup):\n");
    Environment* inner = env_create(global);
    check("child env has parent set", inner->parent == global);

    // x is in global, not in inner - lookup should walk up
    Value* x_from_inner = env_get(inner, "x");
    check("get x from child walks to parent", x_from_inner != NULL);
    check("get x from child finds parent's value",
          x_from_inner && x_from_inner->as.i == 100);

    // ---- Child scope: local binding ----
    env_define(inner, "local_var", value_int(7));
    check("define local_var in child", inner->count == 1);

    Value* local = env_get(inner, "local_var");
    check("get local_var from child finds it locally",
          local && local->as.i == 7);

    // Parent should NOT see child's local
    Value* not_in_parent = env_get(global, "local_var");
    check("parent cannot see child's local binding",
          not_in_parent == NULL);

    // ---- Shadowing ----
    printf("\nShadowing:\n");
    env_define(inner, "x", value_int(999));
    check("shadow x in child - child's count is 2", inner->count == 2);

    Value* shadowed = env_get(inner, "x");
    check("get x from child finds child's binding first",
          shadowed && shadowed->as.i == 999);

    // Parent's x should be untouched
    Value* parent_x = env_get(global, "x");
    check("parent's x is unchanged by child's shadow",
          parent_x && parent_x->as.i == 100);

    // ---- Assignment (walks chain) ----
    printf("\nAssignment:\n");
    // Assign to y (only in global) from child - should walk up and update global's y
    env_assign(inner, "y", value_string("world"));
    check("child count unchanged by assign to parent's binding",
          inner->count == 2);

    Value* updated_y = env_get(global, "y");
    check("assign from child updated parent's binding",
          updated_y && strcmp(updated_y->as.string.chars, "world") == 0);

    // Assign to non-existent - should define in current (child) scope
    env_assign(inner, "brand_new", value_int(50));
    check("assign to non-existent name defines in current scope",
          inner->count == 3);

    Value* new_local = env_get(inner, "brand_new");
    check("newly defined via assign is accessible",
          new_local && new_local->as.i == 50);

    // Parent should NOT see brand_new
    Value* not_here = env_get(global, "brand_new");
    check("newly defined via assign is local only",
          not_here == NULL);

    // ---- Assign updates in same scope ----
    env_assign(inner, "local_var", value_int(77));
    Value* updated_local = env_get(inner, "local_var");
    check("assign updates existing local binding",
          updated_local && updated_local->as.i == 77);

    // ---- Destroy child preserves parent ----
    printf("\nLifecycle:\n");
    env_destroy(inner);
    check("destroy child does not crash", true);

    // Parent should still be functional
    Value* still_there = env_get(global, "x");
    check("parent still accessible after child destroyed",
          still_there && still_there->as.i == 100);

    Value* still_y = env_get(global, "y");
    check("parent's y still 'world' after child destroyed",
          still_y && strcmp(still_y->as.string.chars, "world") == 0);

    // ---- Clean up ----
    env_destroy(global);
    check("destroy global does not crash", true);

    // ---- Summary ----
    printf("\n========== SUMMARY ==========\n");
    printf("  Tests run:    %d\n", tests_run);
    printf("  Tests passed: %d\n", tests_run - tests_failed);
    printf("  Tests failed: %d\n", tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
