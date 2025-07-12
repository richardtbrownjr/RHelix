# Getting Started with RHelix

## Building from Source

```bash
git clone https://github.com/yourusername/rhelix
cd rhelix
make
make test
Memory Management
RHelix provides three memory management strategies:
1. Automatic (Default)
pythondata = load_data()  # Automatically reference counted
2. Arena (Bulk Operations)
python@arena(size="10MB")
def process():
    # Fast allocation, freed all at once
3. Stack (Temporaries)
pythonwith stack[1024]:
    # Ultra-fast stack allocation
Next: Language Syntax
Coming soon: Parser implementation for RHelix syntax!

### 5. **Create GitHub Issues for Next Features**

Go to your repo's Issues tab and create these issues:

**Issue 1: "Implement Basic Parser"**
We need a parser that can handle:

 Basic expressions (numbers, strings, identifiers)
 Function definitions
 Memory annotations (@arena, with stack)
 Type annotations

This will parse .rx files and produce an AST.

**Issue 2: "Design Type System"**
RHelix needs a gradual type system:

 Basic types (int, float, string)
 Generic types (List[T], Array[T])
 Type inference
 Gradual typing (dynamic when needed)


**Issue 3: "Create Language Specification"**
Document the full RHelix syntax:

 Memory management syntax
 Type annotations
 Control flow
 Pattern matching
 Async/await


### 6. **Add GitHub Actions for CI**
Create `.github/workflows/test.yml`:

```yaml
name: Build and Test

on:
  push:
    branches: [ main ]
  pull_request:
    branches: [ main ]

jobs:
  test:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Create build directory
      run: mkdir -p build
    
    - name: Build RHelix runtime
      run: make
    
    - name: Run tests
      run: make test
    
    - name: Check memory leaks
      run: |
        sudo apt-get update
        sudo apt-get install -y valgrind
        valgrind --leak-check=full ./build/test_memory || true
