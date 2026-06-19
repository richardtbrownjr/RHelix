# RHelix - A Modern Language for the AI Era

> 🎓 A learning project to deeply understand programming language design while building something practical for the age of AI-assisted development. I'm building this language as a journey to understand how programming languages really work under the hood. But rather than just building another toy language, I want to create something that addresses a real need: **a language designed from the ground up for the era of AI pair programming**.

### The Learning Goals
- **Understand memory management** by implementing multiple strategies (GC, manual, hybrid)
- **Master parsing and compilation** by building a full compiler pipeline
- **Learn optimization techniques** by making Python-like code run at C++ speeds
- **Explore type systems** by implementing gradual typing with inference

### The Practical Vision
As AI tools become our coding partners, we need languages that:
- Are **readable** enough for AI to understand and generate correctly
- Have **explicit performance controls** that AI can reason about
- Support **gradual refinement** from prototype to production
- Provide **clear error messages** that help both humans and AI debug

## Key Design Principles

### 1. **AI-Friendly Syntax**
```python
# Clear, Python-like syntax that AI models understand well
def process_data(data: List[float]) -> Statistics:
    return calculate_stats(data)

# But with performance hints AI can use
@performance_critical
@arena(size="100MB")  # AI knows this needs memory optimization
def process_large_dataset(data: Dataset) -> Results:
    # Explicit performance intentions
    parallel for batch in data.batches():
        process_batch(batch)
```

### 2. **Progressive Performance**
Start simple, optimize only where needed:
```python
# Version 1: Simple, let AI generate this
def blur_image(img: Image) -> Image:
    return apply_gaussian_blur(img)

# Version 2: AI helps optimize hot paths
@optimize
def blur_image(img: Image) -> Image:
    with arena("10MB"):  # AI suggested this after profiling
        # Optimized implementation
```

### 3. **Memory Management That Makes Sense**
- **Default**: Automatic (like Python) - AI doesn't need to worry about it
- **Explicit**: When you need control - AI can see the intent clearly
```python
# Automatic (default)
data = load_file("big.csv")

# Explicit when needed
with stack[4096]:  # Clear to both human and AI
    temp_buffer = process_chunk(data)
```

## Current Status

### Implemented
- [x] Core memory management system in C
  - Reference counting with cycle detection
  - Arena allocators for bulk operations
  - Stack allocation for temporaries
- [x] Language design for memory annotations
- [x] Lexer with Python-style indentation tracking (INDENT/DEDENT emission, multi-level dedent)
- [x] Full token set: literals, keywords, memory primitives, operators, delimiters, lambdas (`=>`), pipelines (`|>`)
- [x] Tagged-union AST module
- [x] Expression parser with recursive descent and operator precedence
  - Seven precedence levels (equality, comparison, term, factor, unary, call, primary)
  - Left-associative binary operators, right-associative unary
  - Grouping via parentheses
  - First-error-wins error reporting with line and column
- [x] Module + statement parser (assignment, return, expression statement)
- [x] Block parser consuming INDENT/DEDENT
- [x] Control flow statements: if/else, while, for
- [x] Function call expressions (postfix `()` with comma-separated args, chained calls supported)

### In Progress
- [ ] Function and class declarations (`def`, `class`)
- [ ] Decorators and `with` blocks (memory annotations)
- [ ] Subscripts (`arr[i]`) and attribute access (`obj.field`)
- [ ] Type system with gradual typing

### Planned
- [ ] Code generation to C
- [ ] JIT compilation for hot paths
- [ ] AI-friendly error messages
- [ ] Built-in profiling annotations
- [ ] Standard library with AI use cases in mind

## Example: Real-World Image Processing

Here's how the language handles a practical task with progressive optimization:

```python
# Simple version - AI can generate this easily
def process_images(images: List[Image]) -> List[Image]:
    return [enhance(img) for img in images]

# Performance version - AI can optimize when needed
@parallel
def process_images(images: List[Image]) -> List[Image]:
    # AI knows to suggest arena for batch processing
    @arena(size="100MB")
    def process_batch(batch: List[Image]) -> List[Image]:
        results = []
        for img in batch:
            # Stack allocation for temporaries
            with stack[img.size * 4]:
                enhanced = enhance_fast(img)
                results.append(enhanced)
        return results

    # Process in parallel batches
    return parallel_map(process_batch, images.chunks(cpu_count()))
```

## Learning Journey & Blog Posts

I'm documenting what I learn along the way:

## Try It Out

The lexer and a substantial parser are runnable right now. The frontend can parse real algorithmic code — assignments, returns, if/else, while loops, for loops, function calls, and arbitrarily nested combinations of all of these.

```bash
# Clone the repo
git clone https://github.com/richardtbrownjr/RHelix
cd RHelix

# Build the runtime and compiler libraries
make

# Run the memory manager test suite (runtime layer)
make test

# Run the lexer test suite (tokenization, INDENT/DEDENT, full token set)
make test-lexer

# Run the parser test suite (expressions, statements, control flow, calls)
make test-parser
```

The parser tests demonstrate code shapes like:

```python
for item in collection:
    if item > threshold:
        result = process(item, config)
    else:
        log(item)
```

producing fully-formed ASTs with correct precedence, associativity, block nesting, and call resolution. Both test runners pretty-print their output so you can see exactly how source code maps to tokens and ASTs.

### Future (once `def`/`class` and code generation land)
```bash
./rhelix examples/hello.rx
```

## Contributing

This is a learning project, but I'd love to hear your thoughts! Feel free to:
- **Open issues** with suggestions or questions
- **Share resources** about language design
- **Discuss design decisions** in the discussions tab
- **Contribute code** if you're also learning!

## Design Philosophy for AI Era

### What AI Needs from a Language
1. **Clear intent** - Performance annotations make optimization goals explicit
2. **Gradual complexity** - Simple code stays simple, complex only when needed
3. **Error clarity** - Errors that explain *why* not just *what*
4. **Profiling built-in** - AI can see performance data directly

### Example: AI-Assisted Optimization
```python
@profile  # AI can see performance data
def slow_function(data):
    # AI sees this takes 80% of runtime
    result = expensive_operation(data)
    return result

# AI suggests:
@memoize  # Cache results
@parallelize  # Use multiple cores
def optimized_function(data):
    with arena("50MB"):  # Bulk memory allocation
        result = expensive_operation(data)
    return result
```

## Build Status                        
![Build and Test](https://github.com/richardtbrownjr/RHelix/workflows/Build%20and%20Test/badge.svg)

## Recent Progress
- ✅ Core memory management system in C (reference counting + cycle detection + arenas)
- ✅ Runtime test suite
- ✅ Lexer with Python-style indentation (INDENT/DEDENT, multi-level dedent, blank-line handling)
- ✅ Full token set including lambdas (`=>`), pipeline operator (`|>`), and `in` keyword
- ✅ Tagged-union AST module
- ✅ Expression parser with operator precedence, associativity, and grouping
- ✅ Module + statement parser foundation (assignment, return, expression statement)
- ✅ Block parser using INDENT/DEDENT
- ✅ Control flow complete: if/else, while, for
- ✅ Function call expressions with arbitrary arguments (chained calls supported)
- ✅ Parser test suite covering expressions, statements, control flow, calls, and error paths
- 🚧 Function and class declarations (`def`, `class`) — next
- 📋 Subscripts and attribute access (`arr[i]`, `obj.field`)
- 📋 Decorators and `with` blocks (memory annotations)
- 📋 Type system design
- 📋 Code generation to C

## Resources I'm Pulling From

- 📚 "Crafting Interpreters" by Robert Nystrom
- 📚 "Modern Compiler Implementation" by Andrew Appel
- 🔧 Study of: Rust (ownership), Julia (performance), Python (syntax)
- 💡 Conversations with AI about language design

## License

MIT License - Feel free to learn from and build upon this!

---

*"The best way to understand something is to build it. The best time to build a new language is when the world is changing."*

## Contact & Discussion

- GitHub Issues: Questions, suggestions, discussions
- Blog: working on it (detailed write-ups)

---

**Note**: This is an active learning project. Expect things to change as I learn more about language design and compiler construction. The goal is to learn deeply while building something that could actually be useful in our AI-assisted future.
