# RHelix

A custom programming language with Python-like syntax and explicit performance
primitives, implemented in C. A learning project following the architecture
laid out in *Crafting Interpreters*.

## What is RHelix

RHelix is a Python-syntax language with first-class memory and parallelism
annotations. The design goal is to keep the readability and indentation-based
structure of Python while letting the programmer make explicit decisions about
allocation, ownership, and parallel execution where those decisions matter.

- **Familiar surface syntax** — indentation-significant blocks, `def`, `class`,
  `if/else`, `while`, `for ... in`, type annotations
- **Explicit performance primitives** — `@arena`, `@parallel`, stack-allocation
  hints, and a `with` block form for scoped memory regions
- **C runtime** — reference counting with cycle detection, written from scratch
- **Single-pass recursive descent compiler frontend** — no parser generators,
  no external dependencies

This is a learning project, public for documentation and reference. It is not
production-ready and is not intended to be.

## Current Status

The compiler frontend is substantially complete. RHelix source code with
function declarations, type annotations, control flow, data access, classes
with inheritance, decorators, loop control, and boolean logic parses into a
well-formed AST. The frontend is feature-complete relative to the original scope. Semantic analysis is under construction — scope tracking, symbol tables, and name resolution are working. Code generation is not yet implemented.

The repo can parse this without complaint:

```python
@parallel
@arena(1024)
class Buffer(Stream):
    def write(self, data):
        return self.append(data)

@cached
def find_first(items, predicate):
    for item in items:
        if predicate(item) and not item.expired:
            return item
    return None

def withdraw(account, amount):
    if amount > 0 and amount < account.balance and not account.frozen:
        account.balance = account.balance - amount
        return True
    return False
```

## Implemented

### Runtime
- [x] Reference-counted memory manager with cycle detection
- [x] Arena allocator primitives

### Lexer
- [x] Full Python-style indentation tracking (INDENT/DEDENT emission)
- [x] Multi-level dedent handling, blank/comment-line skipping
- [x] EOF dedent closure for unclosed blocks
- [x] Full token set including lambdas (`=>`) and pipelines (`|>`)
- [x] Keyword recognition: `def`, `class`, `if`, `else`, `while`, `for`, `in`,
      `return`, `pass`, `break`, `continue`, `and`, `or`, `not`,
      `True`, `False`, `None`, `with`, `as`

### AST
- [x] Tagged union representation with line/column tracking on every node
- [x] Owned-children memory model with recursive destructor
- [x] Pretty-printer for debugging

### Parser
- [x] Recursive descent with eight precedence levels
      (logical_or, logical_and, equality, comparison, term, factor, unary, postfix)
- [x] Left-associative binary operators, right-associative unary
- [x] Parenthesized grouping
- [x] First-error-wins reporting with line/column information

### Statement parsing
- [x] Module / statement sequence with blank-line tolerance
- [x] Block parser consuming INDENT/DEDENT
- [x] Assignment, return, expression statements
- [x] Control flow: `if/else`, `while`, `for ... in`
- [x] Loop control: `break` and `continue`
- [x] `elif` chains (parsed as nested if/else, no new AST node)
- [x] Assignment to attributes and subscripts (`self.x = v`, `arr[i] = v`)
- [x] Augmented assignment (`+=`, `-=`, `*=`, `/=`, `%=`) with Identifier, Attribute, and Subscript targets
- [x] `with` blocks with optional `as` binding (`with arena(1024) as buf:`)
- [x] Pipeline operator (`|>`) with left-associative chaining (`data |> clean |> transform`)
- [x] Lambda expressions — unparenthesized single-param (`x => body`) and parenthesized multi-param (`(x, y) => body`)
- [x] `pass` statement for empty bodies
- [x] Function declarations (`def`) with parameter and return type annotations
- [x] Compound type annotations (`List[int]`, `Dict[str, int]`, nested forms) via subscript reuse
- [x] Class declarations with method bodies
- [x] Class inheritance (single and multiple base classes)
- [x] Decorators on functions and classes (stacked, with optional arguments)

### Expression parsing
- [x] All arithmetic, comparison, and equality operators
- [x] Logical operators (`and`, `or`, `not`) with correct precedence:
      `not` binds tightest, then `and`, then `or` — comparison binds
      tighter than `and`, matching most C-family languages
- [x] Function call expressions (postfix `()` with comma-separated args)
- [x] Chained calls (`foo()()`)
- [x] Subscripts (`arr[i]`) — chains naturally to `arr[i][j]`
- [x] Collection literals — lists (`[1, 2, 3]`), dicts (`{"a": 1, "b": 2}`), nested and mixed types, trailing commas allowed
- [x] Membership operators (`in`, `not in`) — comparison-level precedence; `not in` handled via two-token lookahead producing `Unary(NOT, Binary(IN, ...))` — no new AST nodes
- [x] Attribute access (`obj.field`) — chains naturally to `obj.a.b.c`
- [x] Method calls (`obj.method(args)`) via Attribute + Call composition
- [x] Free composition of all postfix forms: `obj.method(arg).field[0]`

### Semantic Analysis
- [x] Scope tracking foundation — SemanticAnalyzer walks the AST maintaining a linked-list scope stack
- [x] Scope kinds: MODULE, FUNCTION, CLASS, BLOCK, LAMBDA — each scope-creating construct pushes its own kind
- [x] Push/pop discipline verified balanced across nested structures (tested to depth 5 through class → method → with → if → for chain)
- [x] Exhaustive AST node dispatch — new node types produce compile-time warnings if unhandled
- [x] Symbol tables per scope (linked-list buckets, prepend for shadowing, O(1) insert)
- [x] Symbol kinds — SYM_VARIABLE, SYM_PARAMETER, SYM_FUNCTION, SYM_METHOD, SYM_CLASS — populated during AST walk at 6 sites; methods distinguished from functions by class-scope check
- [x] Name resolution — identifier references looked up in the scope chain; undefined names reported as errors with source location; `error_count` tracked on `SemanticAnalyzer`
- [x] break/continue validation — new SCOPE_LOOP_BODY kind pushed by while/for; is_inside_loop walks parent scopes, stops at function/lambda/class boundaries (Python semantics)
- [x] return validation — is_inside_function walks scope chain looking for SCOPE_FUNCTION or SCOPE_LAMBDA; return outside a function-like scope reports 'return outside function' error with source location

## In Progress

### Frontend (parser)


### Semantic Analysis
- [ ] Type checking against annotations

### Backend
- [ ] Code generation

## Build and Test

Requires `gcc` (or `clang`) and `make`. No other dependencies.

```bash
make             # Build runtime and compiler libraries
make test        # Runtime memory manager test suite
make test-lexer  # Lexer test suite
make test-parser # Parser test suite
make clean       # Remove build artifacts
```

## Project Structure
RHelix/

├── Makefile
├── README.md
├── src/
│   ├── runtime/
│   │   ├── memory_manager.h
│   │   ├── memory_manager.c
│   │   └── test_memory.c
│   └── compiler/
│       ├── token.h
│       ├── token.c
│       ├── lexer.h
│       ├── lexer.c
│       ├── ast.h
│       ├── ast.c
│       ├── parser.h
│       ├── parser.c
│       ├── test_lexer.c
│       └── test_parser.c
└── build/        (gitignored; generated by make)

## Design Decisions

**Tagged union AST.** Every AST node is a `ASTNode` struct with a type tag and
a union of payloads. This is the C idiom for sum types. It costs a switch
statement at every traversal site, but it gives precise memory layout and
catches missing cases via `-Wswitch` warnings.

**Owned-children memory model.** Each AST node owns its children. `ast_destroy`
recursively frees the entire tree top-down. Strings in nodes (identifier
names, string literals) are `strdup`'d on creation. No reference counting on
the AST itself — it is built once during parsing and freed once after use.

**Recursive descent over Pratt parsing.** Each precedence level is a function
that calls the next-higher level. The grammar is encoded in the call structure.
Less elegant than Pratt for very expressive operator sets, but easier to read,
easier to extend, and matches the *Crafting Interpreters* presentation that
this project follows.

**Postfix layer composes uniformly.** Function calls, subscripts, and
attribute access all live in one `call()` function as branches of a single
`while` loop. The loop keeps wrapping the current expression in a new node
as long as it sees `(`, `[`, or `.`. Chains like `obj.method().field[0]`
parse correctly without any special-case code — the same loop runs four
times. Decorators reuse the same `call()` function to parse what follows
`@`, which is why `@name`, `@name(args)`, `@module.name`, and
`@module.name(args)` all work without new parser code.

**Logical operators share existing AST nodes.** `and` and `or` produce
`AST_BINARY` nodes with new token types in the operator field; `not` produces
`AST_UNARY` with `TOKEN_NOT`. No new AST node types were added — the precedence
chain grew by two functions (`logical_or` and `logical_and`) and `unary`
extended its check to include `TOKEN_NOT`. This is a useful reminder that
new operators rarely need new AST shapes — most fit into the binary/unary
buckets that already exist.

**Decorator names are not lexer keywords.** `@arena` and `@parallel` are
significant to RHelix's design, but at parse time they are ordinary
identifiers. The semantic analyzer is where these names will pick up
memory-region and parallelism meanings. Reserving them in the lexer would
prevent legitimate use of `arena` or `parallel` as identifiers in non-decorator
contexts.

**First-error-wins parsing.** When the parser hits an error, it sets a flag
and stops. Later errors are not reported because they are usually noise
cascading from the first one. Panic-mode recovery is a future enhancement.

**No external dependencies.** The compiler is pure C11. Standard library only.
Linked against the project's own runtime library for memory management
utilities.

## Recent Progress

- ✅ Lexer with full indentation handling
- ✅ AST module (tagged union representation)
- ✅ Expression parser with operator precedence
- ✅ Module + statement parser foundation
- ✅ Block parser using INDENT/DEDENT
- ✅ Control flow: `if/else`, `while`, `for` (with `in` keyword)
- ✅ Function call expressions (chained calls supported)
- ✅ Function declarations with parameter and return type annotations
- ✅ Subscripts and attribute access (chained postfix data access)
- ✅ Class declarations with methods and class-level attributes
- ✅ Inheritance and `pass` statement
- ✅ Decorators on functions and classes (uniform postfix expression after `@`)
- ✅ Loop control: `break` and `continue`
- ✅ Logical operators (`and`, `or`, `not`) with precedence integration
- ✅ Assignment to attributes and subscripts (class methods can mutate state)
- ✅ `elif` chains for clean multi-way branching
- ✅ `with` blocks with optional `as` binding (RHelix performance-primitive surface complete)
- ✅ Pipeline operator (`|>`) with correct precedence (arithmetic and logic bind tighter)
- ✅ Lambda expressions — both unparenthesized single-param (`x => body`) and parenthesized multi-param (`(x, y) => body`) forms
- ✅ Parenthesized multi-param lambdas — lookahead-based disambiguation from grouping
- ✅ Semantic analyzer foundation — new compiler phase with scope tracking; five scope kinds, exhaustive AST walker, balanced push/pop verified
- ✅ Compound type annotations (`List[int]`, `Dict[str, int]`, nested forms) — completes original frontend scope
- ✅ Symbol tables per scope with population during AST walk — five symbol kinds, method-vs-function distinction working
- ✅ Name resolution — first user-visible semantic check; undefined names reported with source location
- ✅ Augmented assignment (`+=`, `-=`, `*=`, `/=`, `%=`) — realistic accumulator and state-mutation patterns now parse
- ✅ break/continue validation — first "context check" semantic error; function boundaries respected so break doesn't escape into an outer loop across a def
- ✅ Collection literals — lists, dicts, arbitrary nesting, real serialization patterns (`return {"count": self.count}`) parse cleanly
- ✅ return validation — 'return' now caught outside function/lambda scopes; innermost-function-wins semantics verified via nested-function test
- ✅ Membership operators — `x in items`, `x not in banned`, real permission-check code (`user.role in {"admin": True}`) parses cleanly
- 🚧 Type checking against annotations — the big remaining semantic bite; requires inferring expression types and matching against declared annotations

## License

MIT. See `LICENSE`.

## Author

Richard Brown — [github.com/richardtbrownjr](https://github.com/richardtbrownjr)
