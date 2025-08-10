// test_lexer.c - Test the lexer
#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>

void test_lexer(const char* name, const char* source) {
    printf("\n=== Testing: %s ===\n", name);
    printf("Source:\n%s\n", source);
    printf("Tokens:\n");

    int token_count;
    Token** tokens = lexer_tokenize(source, &token_count);

    for (int i = 0; i < token_count; i++) {
        printf("  ");
        token_print(tokens[i]);
        printf("\n");
        token_destroy(tokens[i]);
    }

    free(tokens);
}

int main() {
    printf("RHelix Lexer Test Suite\n");

    // Test 1: Basic arithmetic
    test_lexer("Arithmetic", "42 + 3.14 * x");

    // Test 2: Function definition
    test_lexer("Function",
        "def add(x: int, y: int) -> int:\n"
        "    return x + y");

    // Test 3: Memory management
    test_lexer("Memory Management",
        "@arena(size=\"10MB\")\n"
        "def process():\n"
        "    buffer = alloc[float](1000)\n"
        "    with stack[4096]:\n"
        "        temp = compute()");

    // Test 4: String and comments
    test_lexer("Strings and Comments",
        "name = \"RHelix\"  # My new language\n"
        "print(f\"Hello, {name}!\")");

    // Test 5: Pipeline operator
    test_lexer("Pipeline",
        "data |> filter(x => x > 0) |> map(sqrt) |> sum()");

    return 0;
}
