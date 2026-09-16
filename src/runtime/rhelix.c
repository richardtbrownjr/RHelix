// rhelix.c - REPL and future main entry point for RHelix
//
// After Session 3b: variables persist across REPL lines.
//   $ ./build/rhelix
//   rhelix> x = 5
//   rhelix> x + 1
//   6
//   rhelix> exit
//
// Reads one line at a time from stdin, parses via parser_parse_module
// (which handles statements AND expressions - the parser wraps bare
// expressions in AST_EXPRESSION_STMT nodes), and evaluates against a
// persistent global Environment.
//
// REPL semantic: if a line is a single expression statement, its
// value is printed. Assignment and other statements execute silently.
//
// Scope after Session 3b:
// - Assignment: x = 5
// - Variable lookup: x + 1
// - If statements: if x > 0: y = 1
// - All expression forms from Session 2
// - Deferred: while/for loops, functions, classes

#include "value.h"
#include "environment.h"
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
        return NULL;
    }
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
    }
    return strdup(buffer);
}

// True if the last non-whitespace character of a line is ':'.
// A trailing ':' signals a Python-style block header (def, if,
// while, for, class, with) and we should read continuation lines.
static bool ends_with_colon(const char* line) {
    if (!line) return false;
    // Scan from end backward, skip trailing whitespace
    int i = (int)strlen(line) - 1;
    while (i >= 0 && (line[i] == ' ' || line[i] == '\t')) i--;
    return i >= 0 && line[i] == ':';
}

// True if the line is empty or only whitespace.
static bool is_empty_line(const char* line) {
    if (!line) return true;
    for (const char* p = line; *p; p++) {
        if (*p != ' ' && *p != '\t') return false;
    }
    return true;
}

// Read a full statement, potentially spanning multiple lines for
// block-structured statements. Returns a malloc'd string with lines
// joined by '\n', or NULL on EOF.
//
// Logic: read one line. If it ends with ':', enter multi-line mode:
// keep reading, showing a '... ' continuation prompt, until we get
// a blank line. Concatenate everything with newlines between.
static char* read_statement(void) {
    char* first = read_line();
    if (!first) return NULL;

    // If the first line doesn't end with ':', return it as-is.
    if (!ends_with_colon(first)) {
        return first;
    }

    // Multi-line mode: accumulate lines until blank
    // Start with the first line
    size_t buf_capacity = 1024;
    size_t buf_len = strlen(first);
    char* buffer = (char*)malloc(buf_capacity);
    if (!buffer) {
        free(first);
        return NULL;
    }
    strcpy(buffer, first);
    free(first);

    while (1) {
        printf("... ");
        fflush(stdout);
        char* line = read_line();
        if (!line) break;  // EOF ends multi-line mode

        if (is_empty_line(line)) {
            free(line);
            break;
        }

        // Append '\n' + line to buffer, growing if needed
        size_t line_len = strlen(line);
        size_t needed = buf_len + 1 + line_len + 1;  // \n + line + \0
        if (needed > buf_capacity) {
            while (buf_capacity < needed) buf_capacity *= 2;
            char* grown = (char*)realloc(buffer, buf_capacity);
            if (!grown) {
                free(line);
                free(buffer);
                return NULL;
            }
            buffer = grown;
        }
        buffer[buf_len] = '\n';
        strcpy(buffer + buf_len + 1, line);
        buf_len += 1 + line_len;
        free(line);
    }

    return buffer;
}

static bool is_blank(const char* line) {
    if (!line) return true;
    for (const char* p = line; *p; p++) {
        if (*p != ' ' && *p != '\t') return false;
    }
    return true;
}

static bool is_exit_command(const char* line) {
    return strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0;
}

int main(void) {
    printf("RHelix REPL — statements + expressions\n");
    printf("Try: x = 5   then   x + 1\n");
    printf("Type 'exit' to quit.\n\n");

    // Persistent global environment - survives across REPL lines.
    Environment* global = env_create(NULL);
    if (!global) {
        fprintf(stderr, "error: could not create global environment\n");
        return 1;
    }

    while (1) {
        printf("rhelix> ");
        fflush(stdout);

        char* line = read_statement();

        if (!line) {
            printf("\n");
            break;
        }

        if (is_blank(line)) {
            free(line);
            continue;
        }

        if (is_exit_command(line)) {
            free(line);
            break;
        }

        // Tokenize
        int token_count = 0;
        Token** tokens = lexer_tokenize(line, &token_count);
        if (!tokens) {
            fprintf(stderr, "error: could not tokenize input\n");
            free(line);
            continue;
        }

        // Parse as module (handles statements AND bare expressions)
        Parser* parser = parser_create(tokens, token_count);
        if (!parser) {
            fprintf(stderr, "error: could not create parser\n");
            for (int i = 0; i < token_count; i++) free(tokens[i]);
            free(tokens);
            free(line);
            continue;
        }

        ASTNode* module = parser_parse_module(parser);

        if (!module) {
            parser_destroy(parser);
            for (int i = 0; i < token_count; i++) free(tokens[i]);
            free(tokens);
            free(line);
            continue;
        }
        
        // Evaluate the module against the persistent global env.
        // evaluate_statement on a module iterates its statements and
        // returns the last statement's value (Python REPL semantic).
        StmtResult sr = evaluate_statement(module, global);
        Value result = sr.value;

        // If the last statement was an expression statement, print
        // the result. Otherwise (assignment, if, etc.), suppress.
        //
        // We detect "last was expression statement" by checking the
        // module's last child directly.
        bool print_result = false;
        if (module->type == AST_MODULE && module->as.module.count > 0) {
            ASTNode* last = module->as.module.statements[module->as.module.count - 1];
            if (last && last->type == AST_EXPRESSION_STMT) {
                print_result = true;
            }
        }

        if (print_result && result.kind != VAL_NONE) {
            char* str = value_to_string(result);
            printf("%s\n", str);
            free(str);
        }

        // Clean up
        value_destroy(&result);
        // NOTE: intentionally leak the module AST at REPL level.
        // Function definitions store pointers into the AST (fn->definition)
        // that need to survive across REPL lines. Freeing here would
        // dangling-pointer the stored function values. Acceptable leak
        // for a learning REPL - future GC or refcounting will fix.
        // ast_destroy(module);
        parser_destroy(parser);
        for (int i = 0; i < token_count; i++) free(tokens[i]);
        free(tokens);
        free(line);
    }

    env_destroy(global);
    printf("Goodbye.\n");
    return 0;
}
