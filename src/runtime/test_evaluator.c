// test_evaluator.c - Tests for tree-walking expression evaluator
//
// These tests construct AST nodes manually (via ast_* constructors),
// pass them to evaluate(), and verify the resulting Values.
//
// This is the first "code actually runs" milestone: 2+3 really produces 5.

#include "evaluator.h"
#include "value.h"
#include "../compiler/ast.h"
#include "../compiler/token.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_run = 0;
static int tests_failed = 0;

// Check that a Value has the expected kind AND numeric-equivalent value.
static void check_int(const char* description, Value got, long long expected) {
    tests_run++;
    if (got.kind == VAL_INT && got.as.i == expected) {
        printf("  ✓ %s → %lld\n", description, got.as.i);
    } else {
        char* got_str = value_to_string(got);
        printf("  ✗ %s → expected VAL_INT %lld, got %s\n",
               description, expected, got_str);
        free(got_str);
        tests_failed++;
    }
    value_destroy(&got);
}

static void check_float(const char* description, Value got, double expected) {
    tests_run++;
    if (got.kind == VAL_FLOAT && got.as.f == expected) {
        printf("  ✓ %s → %g\n", description, got.as.f);
    } else {
        char* got_str = value_to_string(got);
        printf("  ✗ %s → expected VAL_FLOAT %g, got %s\n",
               description, expected, got_str);
        free(got_str);
        tests_failed++;
    }
    value_destroy(&got);
}

static void check_bool(const char* description, Value got, bool expected) {
    tests_run++;
    if (got.kind == VAL_BOOL && got.as.b == expected) {
        printf("  ✓ %s → %s\n", description, got.as.b ? "true" : "false");
    } else {
        char* got_str = value_to_string(got);
        printf("  ✗ %s → expected VAL_BOOL %s, got %s\n",
               description, expected ? "true" : "false", got_str);
        free(got_str);
        tests_failed++;
    }
    value_destroy(&got);
}

static void check_string(const char* description, Value got, const char* expected) {
    tests_run++;
    if (got.kind == VAL_STRING &&
        got.as.string.chars &&
        strcmp(got.as.string.chars, expected) == 0) {
        printf("  ✓ %s → \"%s\"\n", description, got.as.string.chars);
    } else {
        char* got_str = value_to_string(got);
        printf("  ✗ %s → expected VAL_STRING \"%s\", got %s\n",
               description, expected, got_str);
        free(got_str);
        tests_failed++;
    }
    value_destroy(&got);
}

static void check_none(const char* description, Value got) {
    tests_run++;
    if (got.kind == VAL_NONE) {
        printf("  ✓ %s → None\n", description);
    } else {
        char* got_str = value_to_string(got);
        printf("  ✗ %s → expected VAL_NONE, got %s\n",
               description, got_str);
        free(got_str);
        tests_failed++;
    }
    value_destroy(&got);
}

int main(void) {
    printf("\n========== EXPRESSION EVALUATION TESTS ==========\n\n");

    // ---- Literals ----
    printf("Literals:\n");
    check_int("int literal 42", evaluate(ast_literal_int(42, 1, 1)), 42);
    check_int("int literal -1", evaluate(ast_literal_int(-1, 1, 1)), -1);
    check_float("float literal 3.14", evaluate(ast_literal_float(3.14, 1, 1)), 3.14);
    check_string("string literal", evaluate(ast_literal_string("hello", 1, 1)), "hello");
    check_bool("bool literal true", evaluate(ast_literal_bool(true, 1, 1)), true);
    check_bool("bool literal false", evaluate(ast_literal_bool(false, 1, 1)), false);
    check_none("none literal", evaluate(ast_literal_none(1, 1)));

    // ---- Binary arithmetic ----
    printf("\nArithmetic:\n");
    // 2 + 3 = 5   THE FIRST "CODE ACTUALLY RUNS" MOMENT
    ASTNode* two_plus_three = ast_binary(TOKEN_PLUS,
        ast_literal_int(2, 1, 1),
        ast_literal_int(3, 1, 1), 1, 1);
    check_int("2 + 3", evaluate(two_plus_three), 5);
    ast_destroy(two_plus_three);

    ASTNode* ten_minus_four = ast_binary(TOKEN_MINUS,
        ast_literal_int(10, 1, 1),
        ast_literal_int(4, 1, 1), 1, 1);
    check_int("10 - 4", evaluate(ten_minus_four), 6);
    ast_destroy(ten_minus_four);

    ASTNode* six_times_seven = ast_binary(TOKEN_STAR,
        ast_literal_int(6, 1, 1),
        ast_literal_int(7, 1, 1), 1, 1);
    check_int("6 * 7", evaluate(six_times_seven), 42);
    ast_destroy(six_times_seven);

    ASTNode* twenty_div_four = ast_binary(TOKEN_SLASH,
        ast_literal_int(20, 1, 1),
        ast_literal_int(4, 1, 1), 1, 1);
    check_int("20 / 4", evaluate(twenty_div_four), 5);
    ast_destroy(twenty_div_four);

    ASTNode* mod = ast_binary(TOKEN_PERCENT,
        ast_literal_int(17, 1, 1),
        ast_literal_int(5, 1, 1), 1, 1);
    check_int("17 % 5", evaluate(mod), 2);
    ast_destroy(mod);

    // ---- Numeric promotion ----
    printf("\nNumeric promotion:\n");
    ASTNode* int_plus_float = ast_binary(TOKEN_PLUS,
        ast_literal_int(2, 1, 1),
        ast_literal_float(3.5, 1, 1), 1, 1);
    check_float("2 + 3.5 (promotes to float)", evaluate(int_plus_float), 5.5);
    ast_destroy(int_plus_float);

    ASTNode* float_times_int = ast_binary(TOKEN_STAR,
        ast_literal_float(2.5, 1, 1),
        ast_literal_int(4, 1, 1), 1, 1);
    check_float("2.5 * 4", evaluate(float_times_int), 10.0);
    ast_destroy(float_times_int);

    // ---- String concatenation ----
    printf("\nString concat:\n");
    ASTNode* hello_world = ast_binary(TOKEN_PLUS,
        ast_literal_string("hello ", 1, 1),
        ast_literal_string("world", 1, 1), 1, 1);
    check_string("\"hello \" + \"world\"", evaluate(hello_world), "hello world");
    ast_destroy(hello_world);

    // ---- Comparisons ----
    printf("\nComparisons:\n");
    ASTNode* five_gt_three = ast_binary(TOKEN_GREATER,
        ast_literal_int(5, 1, 1),
        ast_literal_int(3, 1, 1), 1, 1);
    check_bool("5 > 3", evaluate(five_gt_three), true);
    ast_destroy(five_gt_three);

    ASTNode* three_gt_five = ast_binary(TOKEN_GREATER,
        ast_literal_int(3, 1, 1),
        ast_literal_int(5, 1, 1), 1, 1);
    check_bool("3 > 5", evaluate(three_gt_five), false);
    ast_destroy(three_gt_five);

    ASTNode* eq = ast_binary(TOKEN_EQUALS_EQUALS,
        ast_literal_int(42, 1, 1),
        ast_literal_int(42, 1, 1), 1, 1);
    check_bool("42 == 42", evaluate(eq), true);
    ast_destroy(eq);

    ASTNode* neq = ast_binary(TOKEN_NOT_EQUALS,
        ast_literal_int(42, 1, 1),
        ast_literal_int(43, 1, 1), 1, 1);
    check_bool("42 != 43", evaluate(neq), true);
    ast_destroy(neq);

    // ---- Short-circuit logical ----
    printf("\nLogical (short-circuit):\n");
    ASTNode* true_and_true = ast_binary(TOKEN_AND,
        ast_literal_bool(true, 1, 1),
        ast_literal_bool(true, 1, 1), 1, 1);
    check_bool("true and true", evaluate(true_and_true), true);
    ast_destroy(true_and_true);

    ASTNode* false_and_x = ast_binary(TOKEN_AND,
        ast_literal_bool(false, 1, 1),
        ast_literal_bool(true, 1, 1), 1, 1);
    check_bool("false and true (short-circuits)", evaluate(false_and_x), false);
    ast_destroy(false_and_x);

    ASTNode* true_or_x = ast_binary(TOKEN_OR,
        ast_literal_bool(true, 1, 1),
        ast_literal_bool(false, 1, 1), 1, 1);
    check_bool("true or false (short-circuits)", evaluate(true_or_x), true);
    ast_destroy(true_or_x);

    // ---- Unary ----
    printf("\nUnary:\n");
    ASTNode* neg_int = ast_unary(TOKEN_MINUS,
        ast_literal_int(42, 1, 1), 1, 1);
    check_int("-42", evaluate(neg_int), -42);
    ast_destroy(neg_int);

    ASTNode* not_true = ast_unary(TOKEN_NOT,
        ast_literal_bool(true, 1, 1), 1, 1);
    check_bool("not true", evaluate(not_true), false);
    ast_destroy(not_true);

    // ---- Ternary ----
    printf("\nTernary:\n");
    ASTNode* ternary_true = ast_ternary(
        ast_literal_int(42, 1, 1),        // then_expr
        ast_literal_bool(true, 1, 1),     // condition
        ast_literal_int(0, 1, 1),         // else_expr
        1, 1);
    check_int("42 if true else 0", evaluate(ternary_true), 42);
    ast_destroy(ternary_true);

    ASTNode* ternary_false = ast_ternary(
        ast_literal_int(42, 1, 1),
        ast_literal_bool(false, 1, 1),
        ast_literal_int(0, 1, 1),
        1, 1);
    check_int("42 if false else 0", evaluate(ternary_false), 0);
    ast_destroy(ternary_false);

    // ---- Nested expressions (composition proof point) ----
    printf("\nComposition:\n");
    // (2 + 3) * 4 = 20
    ASTNode* inner = ast_binary(TOKEN_PLUS,
        ast_literal_int(2, 1, 1),
        ast_literal_int(3, 1, 1), 1, 1);
    ASTNode* nested = ast_binary(TOKEN_STAR,
        inner,
        ast_literal_int(4, 1, 1), 1, 1);
    check_int("(2 + 3) * 4", evaluate(nested), 20);
    ast_destroy(nested);

    // ---- Runtime errors (should return VAL_NONE) ----
    printf("\nRuntime errors:\n");
    // Division by zero
    ASTNode* div_zero = ast_binary(TOKEN_SLASH,
        ast_literal_int(10, 1, 1),
        ast_literal_int(0, 1, 1), 1, 1);
    check_none("10 / 0 (division by zero)", evaluate(div_zero));
    ast_destroy(div_zero);

    // ---- Summary ----
    printf("\n========== SUMMARY ==========\n");
    printf("  Tests run:    %d\n", tests_run);
    printf("  Tests passed: %d\n", tests_run - tests_failed);
    printf("  Tests failed: %d\n", tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
