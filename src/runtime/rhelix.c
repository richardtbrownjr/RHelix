// rhelix.c - REPL and future main entry point for RHelix
//
// Currently: interactive expression evaluator.
//   $ ./build/rhelix
//   rhelix> 2 + 3
//   5
//   rhelix> (10 + 5) * 2
//   30
//   rhelix> exit
//
// Reads one line at a time from stdin, lexes and parses it as an
// expression, evaluates it, prints the result. Exits on EOF (Ctrl+D)
// or when the user types 'exit' or 'quit'.
//
// Scope for the expression-only REPL:
// - Every expression form the evaluator supports works here
// - Assignment, statements, control flow: NOT yet (Session 3+)
// - Multi-line input: NOT yet
// - Line history / arrow keys: NOT yet (no readline dependency)

#include "value.h"
#include "evaluator.h"
#include "../compiler/lexer.h"
#include "../compiler/parser.h"
#include "../compiler/ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Reads a line from stdin. Returns a malloc'd string (caller frees)
// or NULL on EOF. Strips trailing newline.
static char* read_line(void) {
    static char buffer[1024];
    if (!fgets(buffer, sizeof(buffer), stdin)) {
        return NULL;  // EOF or read error
    }
    // Strip trailing newline
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }
    return strdup(buffer);
}

// True if the line is empty or contains only whitespace.
static bool is_blank(const char* line) {
    if (!line) return true;
    for (const char* p = line; *p; p++) {
        if (*p != ' ' && *p != '\t') return false;
    }
    return true;
}

// True if the user typed an exit command.
static bool is_exit_command(const char* line) {
    return strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0;
}

int main(void) {
    printf("RHelix REPL — expression evaluator\n");
    printf("Type expressions like '2 + 3' or 'quit' to exit.\n\n");

    while (1) {
        printf("rhelix> ");
        fflush(stdout);

        char* line = read_line();

        // EOF (Ctrl+D)
        if (!line) {
            printf("\n");
            break;
        }

        // Skip blank lines
        if (is_blank(line)) {
            free(line);
            continue;
        }

        // Exit commands
        if (is_exit_command(line)) {
            free(line);
            break;
        }

        // Tokenize the line
        int token_count = 0;
        Token** tokens = lexer_tokenize(line, &token_count);
        if (!tokens) {
            fprintf(stderr, "error: could not tokenize input\n");
            free(line);
            continue;
        }

        // Create parser from tokens
        Parser* parser = parser_create(tokens, token_count);
        if (!parser) {
            fprintf(stderr, "error: could not create parser\n");
            // Free tokens
            for (int i = 0; i < token_count; i++) {
                free(tokens[i]);
            }
            free(tokens);
            free(line);
            continue;
        }

        // Parse as expression
        ASTNode* expr = parser_parse_expression(parser);

        if (!expr) {
            // Error already printed by parser
            parser_destroy(parser);
            for (int i = 0; i < token_count; i++) {
                free(tokens[i]);
            }
            free(tokens);
            free(line);
            continue;
        }

        // Evaluate
        Value result = evaluate(expr);

        // Print result
        char* str = value_to_string(result);
        printf("%s\n", str);
        free(str);

        // Clean up
        value_destroy(&result);
        ast_destroy(expr);
        parser_destroy(parser);
        for (int i = 0; i < token_count; i++) {
            free(tokens[i]);
        }
        free(tokens);
        free(line);
    }

    printf("Goodbye.\n");
    return 0;
}
