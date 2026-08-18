// test_semantic.c - Foundation test for the semantic analyzer
//
// This test does NOT check for actual semantic errors yet - that comes in
// future sessions. Right now the goal is proving the scaffolding works:
// - The analyzer walks the AST from top to bottom
// - Push/pop discipline is balanced (scope stack empty at end)
// - Scope kinds are pushed correctly for functions, classes, blocks, etc.
// - max_depth_reached grows as we enter nested constructs
//
// If those all hold, the foundation is solid and we can pile checks on top.

#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Runs the full pipeline (lex -> parse -> analyze) on a source string
// and reports what the analyzer observed.
static void run_semantic_case(const char* label, const char* source) {
    printf("\n=== Testing: %s ===\n", label);
    printf("Source:\n%s\n", source);

    // Lex
    Lexer* lexer = lexer_create(source);
    if (!lexer) { printf("  Lexer creation failed\n"); return; }

    Token** tokens = NULL;
    int token_count = 0;
    int token_capacity = 0;
    Token* tok;
    do {
        tok = lexer_next_token(lexer);
        if (!tok) break;
        if (token_count >= token_capacity) {
            int new_cap = token_capacity == 0 ? 16 : token_capacity * 2;
            tokens = (Token**)realloc(tokens, sizeof(Token*) * new_cap);
            token_capacity = new_cap;
        }
        tokens[token_count++] = tok;
    } while (tok->type != TOKEN_EOF);

    // Parse
    Parser* parser = parser_create(tokens, token_count);
    ASTNode* module = parser_parse_module(parser);

    if (!module || parser->had_error) {
        printf("  Parse failed: %s\n",
               parser->had_error ? parser->error_message : "no module");
        goto cleanup;
    }

    // Analyze
    SemanticAnalyzer* sem = semantic_create();
    sem->debug_print_scopes = true;
    bool ok = semantic_analyze(sem, module);

    printf("  Analysis: %s\n", ok ? "OK" : "FAILED");
    printf("  Max scope depth reached: %d\n", sem->max_depth_reached);
    printf("  Errors: %d\n", sem->error_count);
    printf("  Scope stack empty at end: %s\n",
           sem->current_scope == NULL ? "yes" : "NO (imbalance!)");

    semantic_destroy(sem);
    ast_destroy(module);

cleanup:
    parser_destroy(parser);
    for (int i = 0; i < token_count; i++) token_destroy(tokens[i]);
    free(tokens);
    lexer_destroy(lexer);
}

int main(void) {
    printf("========== SEMANTIC ANALYZER FOUNDATION TESTS ==========\n");

    // Baseline: empty module. Should reach depth 0 (MODULE only), stack empty.
    run_semantic_case("Empty module",
        "\n");

    // Simple assignment - no scope changes beyond MODULE.
    run_semantic_case("Top-level assignment",
        "x = 5\n");

    // One function - MODULE (0) -> FUNCTION (1). Max depth 1.
    run_semantic_case("Single function",
        "def foo():\n"
        "    return 1\n");

    // Nested control flow inside a function.
    // MODULE (0) -> FUNCTION (1) -> BLOCK (2) -> BLOCK (3)
    run_semantic_case("Function with nested if/while",
        "def process(items):\n"
        "    if items:\n"
        "        while items:\n"
        "            x = items\n");

    // Class with method - MODULE -> CLASS -> FUNCTION. Max depth 2.
    run_semantic_case("Class with method",
        "class Counter:\n"
        "    def increment(self):\n"
        "        self.value = self.value + 1\n");

    // SESSION PROOF POINT - realistic RHelix code exercising most scope kinds.
    // MODULE -> CLASS -> FUNCTION -> BLOCK (with) -> BLOCK (if) -> ...
    run_semantic_case("Realistic method (proof point)",
        "class Pipeline:\n"
        "    def run(self, data):\n"
        "        with self.lock as l:\n"
        "            if data:\n"
        "                for item in data:\n"
        "                    self.process(item)\n");

    // Lambda inside a function - MODULE -> FUNCTION -> LAMBDA.
    run_semantic_case("Function containing a lambda",
        "def apply(f, x):\n"
        "    return f(x)\n"
        "\n"
        "result = apply(x => x + 1, 10)\n");

    printf("\n\n========== NAME RESOLUTION TESTS ==========\n");

    // ---- Cases that SHOULD PASS (all names defined) ----

    // Simple: LHS defines, RHS uses the defined name in a later statement
    run_semantic_case("Defined name used later (should pass)",
        "x = 5\n"
        "y = x + 1\n");

    // Parameter used inside function body
    run_semantic_case("Parameter used in body (should pass)",
        "def double(n):\n"
        "    return n + n\n");

    // Loop variable used inside body
    run_semantic_case("Loop variable used in body (should pass)",
        "for item in items:\n"
        "    x = item\n");
    // Note: 'items' is undefined - this test expects 1 error, not 0.

    // Class method accessing its own parameter
    run_semantic_case("Method uses self parameter (should pass)",
        "class Counter:\n"
        "    def increment(self):\n"
        "        return self\n");

    // ---- Cases that SHOULD FAIL (undefined names) ----

    // Bare identifier with nothing defined
    run_semantic_case("Undefined name at module level (should error)",
        "y = x + 1\n");

    // Function references undefined name
    run_semantic_case("Undefined name in function body (should error)",
        "def foo():\n"
        "    return bar\n");

    // Nested scope: inner reference to outer name is OK; inner name from
    // outer scope is not
    run_semantic_case("Function param not visible outside (should error)",
        "def foo(x):\n"
        "    return x\n"
        "result = x\n");

    printf("\n========== END NAME RESOLUTION TESTS ==========\n");

    printf("\n\n========== BREAK/CONTINUE VALIDATION TESTS ==========\n");

    // ---- Cases that SHOULD PASS ----

    run_semantic_case("break inside while (should pass)",
        "while True:\n"
        "    break\n");

    run_semantic_case("continue inside while (should pass)",
        "while True:\n"
        "    continue\n");

    run_semantic_case("break inside for (should pass)",
        "for item in items:\n"
        "    break\n");
    // Note: 'items' is undefined - expect 1 name resolution error, 0 loop errors.

    run_semantic_case("break inside nested for-if (should pass)",
        "for item in items:\n"
        "    if item:\n"
        "        break\n");

    // ---- Cases that SHOULD ERROR ----

    run_semantic_case("break at module level (should error)",
        "break\n");

    run_semantic_case("continue at module level (should error)",
        "continue\n");

    run_semantic_case("break inside function outside loop (should error)",
        "def foo():\n"
        "    break\n");

    // Function boundary blocks - break inside inner function does NOT
    // see the outer while loop. This is the classic Python semantics.
    run_semantic_case("break in nested function (should error - function boundary)",
        "while True:\n"
        "    def foo():\n"
        "        break\n");

    printf("\n========== END BREAK/CONTINUE VALIDATION TESTS ==========\n");

    printf("\n\n========== RETURN VALIDATION TESTS ==========\n");

    // ---- Cases that SHOULD PASS ----

    run_semantic_case("return inside function (should pass)",
        "def foo():\n"
        "    return 5\n");

    run_semantic_case("bare return inside function (should pass)",
        "def foo():\n"
        "    return\n");

    run_semantic_case("return with expression inside function (should pass)",
        "def foo(x):\n"
        "    return x + 1\n");

    run_semantic_case("return inside method (should pass)",
        "class Counter:\n"
        "    def get(self):\n"
        "        return self.value\n");

    run_semantic_case("return inside nested function (should pass)",
        "def outer():\n"
        "    def inner():\n"
        "        return 1\n"
        "    return inner\n");

    // ---- Cases that SHOULD ERROR ----

    run_semantic_case("return at module level (should error)",
        "return 5\n");

    run_semantic_case("bare return at module level (should error)",
        "return\n");

    run_semantic_case("return in class body outside method (should error)",
        "class Foo:\n"
        "    return 5\n");

    printf("\n========== END RETURN VALIDATION TESTS ==========\n");
    
    return 0;
}
