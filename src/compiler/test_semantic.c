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
    printf("  Warnings: %d\n", sem->warning_count);
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

    printf("\n\n========== REDECLARATION WARNING TESTS ==========\n");

    // ---- Cases that SHOULD WARN ----

    run_semantic_case("Function redeclared at module level (should warn)",
        "def foo():\n"
        "    return 1\n"
        "def foo():\n"
        "    return 2\n");

    run_semantic_case("Class redeclared at module level (should warn)",
        "class Foo:\n"
        "    pass\n"
        "class Foo:\n"
        "    pass\n");

    run_semantic_case("Method redeclared in class (should warn)",
        "class Counter:\n"
        "    def increment(self):\n"
        "        return 1\n"
        "    def increment(self):\n"
        "        return 2\n");

    run_semantic_case("Function shadows variable (should warn)",
        "x = 5\n"
        "def x():\n"
        "    return 1\n");

    // ---- Cases that should NOT warn ----

    run_semantic_case("Variable reassignment (should NOT warn)",
        "x = 5\n"
        "x = 10\n");

    run_semantic_case("Same-named methods in different classes (should NOT warn)",
        "class A:\n"
        "    def foo(self):\n"
        "        return 1\n"
        "class B:\n"
        "    def foo(self):\n"
        "        return 2\n");

    run_semantic_case("Method shadows top-level function (should NOT warn)",
        "def foo():\n"
        "    return 1\n"
        "class Bar:\n"
        "    def foo(self):\n"
        "        return 2\n");

    run_semantic_case("Nested function (should NOT warn)",
        "def outer():\n"
        "    def inner():\n"
        "        return 1\n"
        "    return inner\n");

    printf("\n========== END REDECLARATION WARNING TESTS ==========\n");

    printf("\n\n========== ARITY CHECK TESTS ==========\n");

// ---- Cases that SHOULD PASS (correct arity) ----

run_semantic_case("Zero-param function called with zero args (should pass)",
    "def foo():\n"
    "    return 1\n"
    "x = foo()\n");

run_semantic_case("Single-param function called with one arg (should pass)",
    "def double(n):\n"
    "    return n + n\n"
    "x = double(5)\n");

run_semantic_case("Multi-param function called correctly (should pass)",
    "def add(a, b):\n"
    "    return a + b\n"
    "x = add(1, 2)\n");

// ---- Cases that SHOULD ERROR (arity mismatch) ----

run_semantic_case("Too few arguments (should error)",
    "def add(a, b):\n"
    "    return a + b\n"
    "x = add(1)\n");

run_semantic_case("Too many arguments (should error)",
    "def double(n):\n"
    "    return n + n\n"
    "x = double(1, 2, 3)\n");

run_semantic_case("Args passed to zero-param function (should error)",
    "def hello():\n"
    "    return 1\n"
    "x = hello(name)\n");

// ---- Cases that SHOULD BE SILENTLY SKIPPED (can't check safely) ----

run_semantic_case("Method call (attribute callee - skipped)",
    "class Counter:\n"
    "    def increment(self):\n"
    "        return 1\n"
    "c = Counter()\n"
    "x = c.increment()\n");
// Note: c.increment is AST_ATTRIBUTE, not AST_IDENTIFIER - skipped.
// Expected: 0 arity errors. Counter() call above is also skipped
// because we don't yet track class 'constructors' as functions.

run_semantic_case("Undefined function call (should error via name resolution, not arity)",
    "x = undefined_func(1, 2)\n");
// Expected: 1 name resolution error, 0 arity errors (lookup returns NULL).

// ---- SESSION PROOF POINT ----
run_semantic_case("Real bug pattern - refactor drops a parameter (proof point)",
    "def calculate(base, rate, term):\n"
    "    return base * rate * term\n"
    "result = calculate(1000, 0.05)\n");
// Expected: 1 arity error - the classic "removed a parameter but
// forgot to update call sites" bug.

printf("\n========== END ARITY CHECK TESTS ==========\n");

printf("\n\n========== TYPE REPRESENTATION TESTS ==========\n");

// ---- Literal inference ----
// These verify type_of_literal maps literals to correct primitive types.

run_semantic_case("Int literal assignment (should show [int])",
    "x = 42\n");

run_semantic_case("Float literal assignment (should show [float])",
    "x = 3.14\n");

run_semantic_case("String literal assignment (should show [str])",
    "x = \"hello\"\n");

run_semantic_case("Bool literal assignment (should show [bool])",
    "x = True\n");

run_semantic_case("None literal assignment (should show [None])",
    "x = None\n");

// ---- Simple annotations ----
// These verify type_from_annotation converts identifier annotations
// to the right primitive types.

run_semantic_case("Function with typed params (should show [int] on each)",
    "def add(a: int, b: int) -> int:\n"
    "    return a + b\n");

run_semantic_case("Function with mixed annotated types (str, bool, float)",
    "def process(name: str, active: bool, ratio: float) -> str:\n"
    "    return name\n");

run_semantic_case("Function with no annotations (all params should show [any])",
    "def untyped(a, b):\n"
    "    return a\n");

// ---- Compound annotations ----
// These exercise type_from_annotation on subscript-shaped types.

run_semantic_case("Function with List[int] param (should show [List[int]])",
    "def process(items: List[int]):\n"
    "    return items\n");

run_semantic_case("Function with Dict[str, int] param (should show [Dict[str, any]])",
    "def lookup(m: Dict[str, int]) -> int:\n"
    "    return 0\n");

run_semantic_case("Function with nested generic (List[Dict[str, int]])",
    "def process(nested: List[Dict[str, int]]):\n"
    "    return nested\n");

// ---- Function type composition ----

run_semantic_case("Complex function signature (proof point)",
    "def transform(data: List[int], key: str, opts: Dict[str, bool]) -> Dict[str, int]:\n"
    "    return {}\n");
// Expected: transform symbol should show
//   [(List[int], str, Dict[str, any]) -> Dict[str, any]]
// (Dict shows [K, any] due to compound-annotation limitation.)

// ---- Class methods with annotations ----

run_semantic_case("Method with typed self/param (proof point)",
    "class Calculator:\n"
    "    def add(self, n: int) -> int:\n"
    "        return n\n");
// Expected: add symbol in CLASS scope should be METHOD with
//   [(any, int) -> int]  (self is unannotated, hence any)

printf("\n========== END TYPE REPRESENTATION TESTS ==========\n");

printf("\n\n========== EXPRESSION TYPE INFERENCE TESTS ==========\n");

// ---- Assignment RHS inference beyond literals ----
// These are the tests that Session 2 makes possible.

run_semantic_case("Assignment from arithmetic (should show [int])",
    "x = 2 + 3\n");

run_semantic_case("Assignment from float promotion (should show [float])",
    "x = 2 + 3.5\n");

run_semantic_case("Assignment from string concat (should show [str])",
    "x = \"hello\" + \" world\"\n");

run_semantic_case("Assignment from comparison (should show [bool])",
    "x = 5 > 3\n");

run_semantic_case("Assignment from logical (should show [bool])",
    "x = True and False\n");

run_semantic_case("Assignment from annotated function call (should show [int])",
    "def add(a: int, b: int) -> int:\n"
    "    return a + b\n"
    "result = add(1, 2)\n");
// The [int] should propagate: add's declared return type is int,
// call inherits that, x gets [int].

run_semantic_case("Assignment from unannotated function call (should show [any])",
    "def foo(a, b):\n"
    "    return a + b\n"
    "result = foo(1, 2)\n");

run_semantic_case("Assignment from list literal (should show [List[int]])",
    "items = [1, 2, 3]\n");

run_semantic_case("Assignment from mixed list literal (should show [List[any]])",
    "items = [1, \"two\", True]\n");

run_semantic_case("Assignment from dict literal (should show [Dict[str, int]])",
    "config = {\"port\": 8080, \"timeout\": 30}\n");

// ---- Arithmetic type errors ----

run_semantic_case("Int + str (should error)",
    "x = 5 + \"hello\"\n");

run_semantic_case("Str - str (should error - subtraction not defined for strings)",
    "x = \"a\" - \"b\"\n");

run_semantic_case("Unary minus on string (should error)",
    "x = -\"hello\"\n");

run_semantic_case("Bool arithmetic (should error)",
    "x = True + False\n");

// ---- Errors inside return expressions ----

run_semantic_case("Type error inside return (should error)",
    "def broken(a: int, b: str) -> int:\n"
    "    return a + b\n");
// The int + str error fires from inside the return expression.

// ---- Nested expression inference ----

run_semantic_case("Nested arithmetic (should show [float])",
    "x = 1 + 2 * 3.14\n");

run_semantic_case("Grouping doesn't change type",
    "x = (1 + 2) * 3\n");

// ---- Function call propagation ----

run_semantic_case("Chained call inference (should show [int])",
    "def double(n: int) -> int:\n"
    "    return n + n\n"
    "def triple(n: int) -> int:\n"
    "    return n + n + n\n"
    "x = double(triple(5))\n");
// triple(5) -> int, double(int) -> int, x -> int.

// ---- SESSION PROOF POINT ----

run_semantic_case("Real code: typed function pipeline (proof point)",
    "def parse(text: str) -> int:\n"
    "    return 0\n"
    "def format(n: int) -> str:\n"
    "    return \"\"\n"
    "result = format(parse(\"42\"))\n");
// parse(str) -> int, format(int) -> str, result -> str.
// Every step type-safe.

printf("\n========== END EXPRESSION TYPE INFERENCE TESTS ==========\n");

printf("\n\n========== ASSIGNMENT TYPE CHECKING TESTS ==========\n");

// ---- Cases that SHOULD PASS ----

run_semantic_case("Typed param reassigned to same type (should pass)",
    "def foo(x: int):\n"
    "    x = 5\n");

run_semantic_case("Typed param reassigned to unknown (should pass - ANY wildcard)",
    "def foo(x: int, y):\n"
    "    x = y\n");
// y is untyped (ANY), so reassignment is permitted.

run_semantic_case("Untyped param reassigned (should pass)",
    "def foo(x, y):\n"
    "    x = y\n");

run_semantic_case("Module-level variable retype (should pass - no declared type)",
    "x = 5\n"
    "x = \"hello\"\n");
// Module-level variables have no declared type, so reassignment
// to any type is legal. Matches Python's dynamic behavior.

run_semantic_case("Typed param reassigned to matching call result (should pass)",
    "def make_int() -> int:\n"
    "    return 42\n"
    "def use(x: int):\n"
    "    x = make_int()\n");

// ---- Cases that SHOULD ERROR ----

run_semantic_case("Typed int param reassigned to string (should error)",
    "def foo(x: int):\n"
    "    x = \"hello\"\n");

run_semantic_case("Typed str param reassigned to int (should error)",
    "def format(name: str):\n"
    "    name = 42\n");

run_semantic_case("Typed float param reassigned to bool (should error)",
    "def calc(ratio: float):\n"
    "    ratio = True\n");

run_semantic_case("Typed param reassigned to wrong call result (should error)",
    "def make_str() -> str:\n"
    "    return \"hello\"\n"
    "def use(x: int):\n"
    "    x = make_str()\n");

// ---- Compound type reassignment ----

run_semantic_case("Typed List[int] param reassigned to matching list (should pass)",
    "def process(items: List[int]):\n"
    "    items = [1, 2, 3]\n");

run_semantic_case("Typed List[int] param reassigned to wrong list (should error)",
    "def process(items: List[int]):\n"
    "    items = [\"a\", \"b\"]\n");

// ---- SESSION PROOF POINT ----

run_semantic_case("Real bug pattern - refactor changes annotation (proof point)",
    "def calculate(rate: float) -> float:\n"
    "    rate = \"5%\"\n"
    "    return 0.0\n");
// The classic "someone changed rate: int to rate: float but forgot
// to remove a temp string reassignment from an old debug session"
// bug. Caught at parse time.

printf("\n========== END ASSIGNMENT TYPE CHECKING TESTS ==========\n");

    return 0;
}
