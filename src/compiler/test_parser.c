// test_parser.c - Test suite for the RHelix parser
#include "parser.h"
#include "lexer.h"
#include "ast.h"
#include <stdio.h>
#include <stdlib.h>

static void test_parser_case(const char* name, const char* source) {
    printf("\n=== Testing: %s ===\n", name);
    printf("Source: %s\n", source);

    int token_count;
    Token** tokens = lexer_tokenize(source, &token_count);
    if (!tokens) {
        printf("Lexer failed\n");
        return;
    }

    Parser* parser = parser_create(tokens, token_count);
    ASTNode* ast = parser_parse_expression(parser);

    if (parser->had_error) {
        printf("Error: %s\n", parser->error_message);
    } else if (ast) {
        printf("AST:\n");
        ast_print(ast, 1);
    } else {
        printf("(no AST returned, no error reported)\n");
    }

    if (ast) ast_destroy(ast);
    parser_destroy(parser);
    for (int i = 0; i < token_count; i++) {
        token_destroy(tokens[i]);
    }
    free(tokens);
}

static void test_module_case(const char* name, const char* source) {
    printf("\n=== Testing: %s ===\n", name);
    printf("Source:\n%s\n", source);

    int token_count;
    Token** tokens = lexer_tokenize(source, &token_count);
    if (!tokens) {
        printf("Lexer failed\n");
        return;
    }

    Parser* parser = parser_create(tokens, token_count);
    ASTNode* ast = parser_parse_module(parser);

    if (parser->had_error) {
        printf("Error: %s\n", parser->error_message);
    } else if (ast) {
        printf("AST:\n");
        ast_print(ast, 1);
    } else {
        printf("(no AST returned, no error reported)\n");
    }

    if (ast) ast_destroy(ast);
    parser_destroy(parser);
    for (int i = 0; i < token_count; i++) {
        token_destroy(tokens[i]);
    }
    free(tokens);
}

int main(void) {
    printf("RHelix Parser Test Suite\n");

    // ===== Literals =====
    test_parser_case("Integer literal", "42");
    test_parser_case("Float literal", "3.14");
    test_parser_case("String literal", "\"hello\"");
    test_parser_case("True literal", "True");
    test_parser_case("False literal", "False");
    test_parser_case("None literal", "None");
    test_parser_case("Identifier", "x");

    // ===== Operator precedence (the big proof) =====
    test_parser_case("Addition", "1 + 2");
    test_parser_case("Multiplication binds tighter than addition", "2 + 3 * 4");
    test_parser_case("Parens override precedence", "(2 + 3) * 4");

    // ===== Associativity =====
    test_parser_case("Subtraction is left-associative", "10 - 3 - 2");
    test_parser_case("Unary is right-associative", "--5");

    // ===== Comparison and equality =====
    test_parser_case("Comparison", "x > 0");
    test_parser_case("Equality", "42 == 42");
    test_parser_case("Comparison binds tighter than equality", "x > 0 == True");

    // ===== Unary =====
    test_parser_case("Unary minus on identifier", "-x");
    test_parser_case("Unary minus on grouped expression", "-(x + y)");

    // ===== Mixed =====
    test_parser_case("Mixed precedence", "1 + 2 * 3 - 4 / 2");

    // ===== Error cases =====
    test_parser_case("Missing right operand", "42 +");
    test_parser_case("Missing closing paren", "(1 + 2");
    test_parser_case("Just a stray operator", "+");
    // ===== Module / statement parser =====
    printf("\n\n========== MODULE PARSER TESTS ==========\n");

    test_module_case("Empty module", "");
    test_module_case("Single expression statement", "42");
    test_module_case("Single assignment", "x = 42");
    test_module_case("Return with value", "return x + 1");
    test_module_case("Bare return", "return");

    test_module_case("Three assignments",
        "x = 1\n"
        "y = 2\n"
        "z = x + y\n");

    test_module_case("Mixed statement types",
        "x = 42\n"
        "y > 0\n"
        "return x * y\n");

    test_module_case("Blank lines between statements",
        "x = 1\n"
        "\n"
        "\n"
        "y = 2\n");

    test_module_case("Function-shaped (no def yet)",
        "x = 10\n"
        "y = 20\n"
        "return x + y\n");

    test_module_case("Error: assignment with no value", "x = ");
    test_module_case("Error: bare operator in expression statement", "+");

    // ===== Compound statements (blocks, if/else, while) =====
    printf("\n\n========== COMPOUND STATEMENT TESTS ==========\n");

    test_module_case("Simple if",
        "if x > 0:\n"
        "    y = 1\n");

    test_module_case("If with else",
        "if x > 0:\n"
        "    y = 1\n"
        "else:\n"
        "    y = 2\n");

    test_module_case("Nested if",
        "if x > 0:\n"
        "    if y > 0:\n"
        "        return 1\n");

    test_module_case("If with multi-statement block",
        "if x > 0:\n"
        "    a = 1\n"
        "    b = 2\n"
        "    return a + b\n");

    test_module_case("While loop",
        "while x < 10:\n"
        "    x = x + 1\n");

    test_module_case("While with multi-statement body",
        "while x < 10:\n"
        "    y = x * 2\n"
        "    x = x + 1\n");

    test_module_case("Module with if at end",
        "x = 10\n"
        "y = 20\n"
        "if x > y:\n"
        "    return x\n"
        "else:\n"
        "    return y\n");

    test_module_case("Error: missing colon after if condition",
        "if x > 0\n"
        "    y = 1\n");

    test_module_case("Error: missing indented block",
        "if x > 0:\n"
        "y = 1\n");

        // ===== Function calls =====
        printf("\n\n========== FUNCTION CALL TESTS ==========\n");

        test_parser_case("Zero-arg call", "foo()");
        test_parser_case("One-arg call", "foo(42)");
        test_parser_case("Multi-arg call", "foo(1, 2, 3)");
        test_parser_case("Call with expression args", "foo(x + 1, y * 2)");
        test_parser_case("Nested call", "print(add(1, 2))");
        test_parser_case("Chained calls", "foo()()");
        test_parser_case("Error: missing closing paren in call", "foo(1, 2");

        test_module_case("Bare call as statement", "foo()\n");
        test_module_case("Call as assignment value", "x = compute(10, 20)\n");

        // ===== For loops =====
        printf("\n\n========== FOR LOOP TESTS ==========\n");

        test_module_case("Simple for",
            "for i in items:\n"
            "    x = i\n");

        test_module_case("For with multi-statement body",
            "for i in items:\n"
            "    a = i * 2\n"
            "    b = a + 1\n");

        test_module_case("Nested for",
            "for i in outer:\n"
            "    for j in inner:\n"
            "        x = i + j\n");

        test_module_case("For with call in body (combined proof)",
            "for i in items:\n"
            "    print(i + 1)\n");

        test_module_case("Error: for without in",
            "for i = items:\n"
            "    x = i\n");

        test_module_case("Error: for without identifier",
            "for in items:\n"
            "    x = i\n");

    return 0;
}
