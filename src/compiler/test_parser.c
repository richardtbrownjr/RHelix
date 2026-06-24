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
            // ===== Function declarations =====
        printf("\n\n========== FUNCTION DECLARATION TESTS ==========\n");

        test_module_case("Zero-param def",
            "def hello():\n"
            "    return 1\n");

        test_module_case("Single-param def",
            "def square(x):\n"
            "    return x * x\n");

        test_module_case("Multi-param def",
            "def add(x, y):\n"
            "    return x + y\n");

        test_module_case("Def with type annotations",
            "def add(x: int, y: int):\n"
            "    return x + y\n");

        test_module_case("Def with return type",
            "def add(x, y) -> int:\n"
            "    return x + y\n");

        test_module_case("Def with param types and return type",
            "def add(x: int, y: int) -> int:\n"
            "    return x + y\n");

        // SESSION PROOF POINT: recursive function with full signature
        test_module_case("Recursive fibonacci (session proof point)",
            "def fibonacci(n: int) -> int:\n"
            "    if n < 2:\n"
            "        return n\n"
            "    return fibonacci(n - 1) + fibonacci(n - 2)\n");

        test_module_case("Def followed by other statements",
            "def double(x):\n"
            "    return x * 2\n"
            "result = double(5)\n");

        // Error cases
        test_module_case("Error: missing function name",
            "def ():\n"
            "    return 1\n");

        test_module_case("Error: missing opening paren",
            "def foo:\n"
            "    return 1\n");

        test_module_case("Error: missing colon",
            "def foo()\n"
            "    return 1\n");

        test_module_case("Error: malformed param (comma at start)",
            "def foo(,):\n"
            "    return 1\n");

        test_module_case("Error: type annotation missing type",
            "def foo(x: ):\n"
            "    return 1\n");

        test_module_case("Error: arrow but no return type",
            "def foo() ->:\n"
            "    return 1\n");
            // ===== Subscripts and attribute access =====
        printf("\n\n========== SUBSCRIPT & ATTRIBUTE TESTS ==========\n");

        // Subscripts
        test_parser_case("Simple subscript", "arr[0]");
        test_parser_case("Subscript with expression index", "arr[i + 1]");
        test_parser_case("Chained subscript (2D)", "grid[i][j]");
        test_parser_case("Subscript with call as index", "arr[len(x)]");

        // Attributes
        test_parser_case("Simple attribute", "obj.field");
        test_parser_case("Chained attribute", "obj.field.subfield");
        test_parser_case("Attribute then call", "obj.method()");
        test_parser_case("Attribute then call with args", "obj.method(1, 2)");

        // Mixed chains
        test_parser_case("Subscript then attribute", "arr[0].name");
        test_parser_case("Attribute then subscript", "obj.items[0]");
        test_parser_case("Call then attribute", "build().result");
        test_parser_case("Call then subscript", "make_list()[0]");

        // SESSION PROOF POINT: four postfix wrappers in one expression
        test_parser_case("Mixed chain proof point",
                         "obj.method(arg).field[0]");

        test_parser_case("Realistic data access",
                         "data[key].process(value)");

        // Subscripts and attributes inside statements
        test_module_case("Subscript in assignment value",
            "x = grid[i][j]\n");

        test_module_case("Attribute in assignment value",
            "name = user.profile.name\n");

        test_module_case("Subscript in return",
            "def first(items):\n"
            "    return items[0]\n");

        test_module_case("Attribute in return",
            "def get_name(user):\n"
            "    return user.name\n");

        test_module_case("Mixed access in function body",
            "def process(records):\n"
            "    first = records[0].value\n"
            "    return first.transform(records[1].config)\n");

        test_module_case("Subscript in for iterable",
            "for item in data.items:\n"
            "    process(item)\n");

        // Error cases
        test_parser_case("Error: empty subscript", "arr[]");
        test_parser_case("Error: missing close bracket", "arr[0");
        test_parser_case("Error: dot with no identifier", "obj.");
        test_parser_case("Error: dot followed by number", "obj.123");

        // ===== Class declarations =====
    printf("\n\n========== CLASS DECLARATION TESTS ==========\n");

    test_module_case("Class with single method",
        "class Counter:\n"
        "    def increment(self):\n"
        "        return self.value + 1\n");

    test_module_case("Class with multiple methods",
        "class Counter:\n"
        "    def increment(self):\n"
        "        return self.value + 1\n"
        "    def get(self):\n"
        "        return self.value\n");

    test_module_case("Class with class-level attribute",
        "class Config:\n"
        "    default_size = 100\n");

    test_module_case("Class with attributes and methods mixed",
        "class Point:\n"
        "    dimensions = 2\n"
        "    def get_x(self):\n"
        "        return self.x\n"
        "    def get_y(self):\n"
        "        return self.y\n");

    test_module_case("Class with method using self and control flow",
        "class Validator:\n"
        "    def check(self, value):\n"
        "        if value > 0:\n"
        "            return True\n"
        "        return False\n");

    // SESSION PROOF POINT: class + instantiation + method call
    test_module_case("Class definition and usage (session proof point)",
        "class Counter:\n"
        "    def increment(self):\n"
        "        return self.value + 1\n"
        "    def get(self):\n"
        "        return self.value\n"
        "c = Counter()\n"
        "result = c.increment()\n");

    test_module_case("Multiple classes in same module",
        "class Animal:\n"
        "    def speak(self):\n"
        "        return self.sound\n"
        "class Counter:\n"
        "    def tick(self):\n"
        "        return self.n + 1\n");

    test_module_case("Class with typed method (combining features)",
        "class Calculator:\n"
        "    def add(self, x: int, y: int) -> int:\n"
        "        return x + y\n");

    // Error cases
    test_module_case("Error: missing class name",
        "class :\n"
        "    x = 1\n");

    test_module_case("Error: missing colon",
        "class Foo\n"
        "    x = 1\n");

    test_module_case("Error: missing class body",
        "class Foo:\n");
        // ===== Inheritance and pass =====
        printf("\n\n========== INHERITANCE AND PASS TESTS ==========\n");

        // Pass statement
        test_module_case("Pass as standalone statement",
            "pass\n");

        test_module_case("Class with empty body (pass)",
            "class Empty:\n"
            "    pass\n");

        test_module_case("Function with empty body (pass)",
            "def todo():\n"
            "    pass\n");

        test_module_case("Pass mixed with other statements",
            "def stub():\n"
            "    x = 1\n"
            "    pass\n"
            "    return x\n");

        // Inheritance
        test_module_case("Single inheritance",
            "class Dog(Animal):\n"
            "    def fetch(self):\n"
            "        return self.toy\n");

        test_module_case("Multiple inheritance",
            "class Hybrid(Animal, Trackable, Serializable):\n"
            "    pass\n");

        test_module_case("Inheritance with empty body using pass",
            "class Dog(Animal):\n"
            "    pass\n");

        test_module_case("Empty parens equals no bases",
            "class Foo():\n"
            "    pass\n");

        // SESSION PROOF POINT: realistic small class hierarchy
        test_module_case("Class hierarchy (session proof point)",
            "class Animal:\n"
            "    def speak(self):\n"
            "        return self.sound\n"
            "class Dog(Animal):\n"
            "    def fetch(self):\n"
            "        return self.toy\n"
            "class Puppy(Dog):\n"
            "    pass\n"
            "p = Puppy()\n"
            "result = p.fetch()\n");

        // Error cases
        test_module_case("Error: missing base class name after comma",
            "class Foo(A,):\n"
            "    pass\n");

        test_module_case("Error: missing close paren in bases",
            "class Foo(A, B:\n"
            "    pass\n");
            // ===== Decorators =====
        printf("\n\n========== DECORATOR TESTS ==========\n");

        test_module_case("Simple decorator on function",
            "@cached\n"
            "def expensive(x):\n"
            "    return x\n");

        test_module_case("Decorator with arguments on function",
            "@arena(100)\n"
            "def matmul(a, b):\n"
            "    return a\n");

        test_module_case("Multiple decorators stacked",
            "@cached\n"
            "@logged\n"
            "@traced\n"
            "def critical(x):\n"
            "    return x\n");

        test_module_case("Decorator on class",
            "@dataclass\n"
            "class User:\n"
            "    pass\n");

        test_module_case("Decorator on class with inheritance",
            "@dataclass\n"
            "class Admin(User):\n"
            "    pass\n");

        test_module_case("Attribute-style decorator",
            "@functools.cache\n"
            "def fib(n):\n"
            "    return n\n");

        test_module_case("Attribute decorator with args",
            "@functools.lru_cache(128)\n"
            "def expensive(x):\n"
            "    return x\n");

        // SESSION PROOF POINT: RHelix-style stacked decorators on inheritance
        test_module_case("Decorator proof point (RHelix-style)",
            "@parallel\n"
            "@arena(1024)\n"
            "class Buffer(Stream):\n"
            "    def write(self, data):\n"
            "        return self.append(data)\n");

        // Error cases
        test_module_case("Error: decorator with no target",
            "@cached\n");

        test_module_case("Error: decorator followed by non-def/class",
            "@cached\n"
            "x = 1\n");

        test_module_case("Error: missing expression after @",
            "@\n"
            "def foo():\n"
            "    return 1\n");
            // ===== break and continue =====
      printf("\n\n========== BREAK AND CONTINUE TESTS ==========\n");

      test_module_case("Break in while loop",
          "while x < 10:\n"
          "    if x > 5:\n"
          "        break\n"
          "    x = x + 1\n");

      test_module_case("Continue in while loop",
          "while x < 10:\n"
          "    if x > 5:\n"
          "        continue\n"
          "    x = x + 1\n");

      test_module_case("Break in for loop",
          "for item in items:\n"
          "    if item > threshold:\n"
          "        break\n");

      test_module_case("Continue in for loop",
          "for item in items:\n"
          "    if item < 0:\n"
          "        continue\n"
          "    process(item)\n");

      test_module_case("Both break and continue in same loop",
          "for n in numbers:\n"
          "    if n < 0:\n"
          "        continue\n"
          "    if n > 100:\n"
          "        break\n"
          "    total = total + n\n");

      // SESSION PROOF POINT: realistic search with break
      test_module_case("Linear search with break (proof point)",
          "def find(items, target):\n"
          "    for item in items:\n"
          "        if item == target:\n"
          "            return item\n"
          "    return None\n");

      test_module_case("Filter pattern with continue (proof point)",
          "def positive_only(items):\n"
          "    for item in items:\n"
          "        if item < 0:\n"
          "            continue\n"
          "        result.add(item)\n"
          "    return result\n");

      // Note: the parser does not enforce that break/continue appear
      // only inside loops. That is a semantic check that belongs to a
      // future analysis pass, not the parser.
      test_module_case("Break at module level (parser accepts; semantics later)",
          "break\n");
          // ===== Logical operators (and / or / not) =====
              printf("\n\n========== LOGICAL OPERATOR TESTS ==========\n");

              // Simple cases
              test_parser_case("Simple and", "x and y");
              test_parser_case("Simple or", "x or y");
              test_parser_case("Simple not", "not x");

              // Precedence: and binds tighter than or
              test_parser_case("And binds tighter than or",
                               "a or b and c");

              // Precedence: comparison binds tighter than and
              test_parser_case("Comparison binds tighter than and",
                               "x > 0 and y < 10");

              // Three-way logical chain
              test_parser_case("Three-way and chain",
                               "a and b and c");

              test_parser_case("Three-way or chain",
                               "a or b or c");

              // Not with comparison
              test_parser_case("Not with comparison",
                               "not x == y");

              // Double not
              test_parser_case("Double not", "not not x");

              // SESSION PROOF POINT: realistic guard expression
              test_parser_case("Range check (proof point)",
                               "x > 0 and x < 100");

              test_parser_case("Mixed guard (proof point)",
                               "user.active and not user.banned or admin");

              // Inside statements
              test_module_case("Logical in if condition",
                  "if x > 0 and x < 10:\n"
                  "    return x\n");

              test_module_case("Logical in while condition",
                  "while running and not done:\n"
                  "    step()\n");

              test_module_case("Filter with compound guard",
                  "for item in items:\n"
                  "    if item.valid and not item.expired:\n"
                  "        process(item)\n");
    return 0;
}
