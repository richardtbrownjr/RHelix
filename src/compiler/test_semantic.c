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
    bool ok = semantic_analyze(sem, module);

    printf("  Analysis: %s\n", ok ? "OK" : "FAILED");
    printf("  Max scope depth reached: %d\n", sem->max_depth_reached);
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

    printf("\n========== ALL TESTS COMPLETE ==========\n");
    return 0;
}
