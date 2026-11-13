# Zombie Scheme Standard Library

This directory contains Scheme library modules that can be loaded by users at startup or on-demand.

## Quick Start

### Load at startup

```bash
# Method 1: Redirect init file to REPL
./x64/Debug/REPL-test.exe < stdlib/init.scm

# Method 2: Load from within REPL
> (load "stdlib/init.scm")
```

### Use libraries in your code

```scheme
;; Load a library definition
(load "stdlib/srfi-1-lists.scm")

;; Import the library
(import (srfi 1))

;; Use library functions
(iota 10)                    ; => (0 1 2 3 4 5 6 7 8 9)
(filter even? '(1 2 3 4))   ; => (2 4)
(fold + 0 '(1 2 3 4))       ; => 10
```

## Available Libraries

### init.scm - Startup Utilities

Essential utilities loaded at startup. Provides:

**Math:** `square`, `cube`, `average`, `sum`, `product`, `abs`, `even?`, `odd?`, `max-list`, `min-list`

**Lists:** `range`, `iota`, `take`, `drop`, `filter`, `foldl`, `foldr`, `any`, `all`, `zip`, `last`, `flatten`

**Strings:** `string-join`

**Debug:** `pp` (pretty print), `debug`

**Functional:** `compose`, `curry`, `identity`

**Usage:**
```scheme
(load "stdlib/init.scm")
(square 5)           ; => 25
(range 0 10)         ; => (0 1 2 3 4 5 6 7 8 9)
(filter odd? (range 0 10))  ; => (1 3 5 7 9)
```

### srfi-1-lists.scm - Extended List Operations

SRFI-1 style list library with powerful list operations.

**Module:** `(srfi 1)`

**Exports:**
- Constructors: `iota`, `circular-list`
- Predicates: `proper-list?`, `circular-list?`, `dotted-list?`
- Selectors: `first`, `second`, `third`, `fourth`, `fifth`, `take`, `drop`, `take-right`, `drop-right`, `last`, `last-pair`
- Misc: `concatenate`, `zip`, `unzip1`, `unzip2`, `count`
- Folding: `fold`, `fold-right`, `reduce`, `reduce-right`
- Filtering: `filter`, `partition`, `remove`
- Searching: `find`, `find-tail`, `any`, `every`
- Alists: `alist-cons`, `alist-delete`

**Usage:**
```scheme
(load "stdlib/srfi-1-lists.scm")
(import (srfi 1))

;; Generate sequences
(iota 5)                    ; => (0 1 2 3 4)
(iota 5 10 2)              ; => (10 12 14 16 18)

;; List predicates
(proper-list? '(1 2 3))    ; => #t
(dotted-list? '(1 . 2))    ; => #t

;; Selectors
(take '(1 2 3 4 5) 3)      ; => (1 2 3)
(drop '(1 2 3 4 5) 3)      ; => (4 5)
(take-right '(1 2 3 4) 2)  ; => (3 4)

;; Folding
(fold + 0 '(1 2 3 4))      ; => 10
(fold-right cons '() '(1 2 3))  ; => (1 2 3)

;; Filtering
(filter even? '(1 2 3 4 5 6))     ; => (2 4 6)
(partition even? '(1 2 3 4 5 6))  ; => ((2 4 6) (1 3 5))
(remove odd? '(1 2 3 4 5 6))      ; => (2 4 6)

;; Searching
(find even? '(1 3 5 6 7))  ; => 6
(any even? '(1 3 5 6))     ; => #t
(every odd? '(1 3 5))      ; => #t

;; Alists
(alist-cons 'x 10 '((y . 20)))  ; => ((x . 10) (y . 20))
```

### utils.scm - General Utilities

Common utility functions for everyday programming.

**Module:** `(zombie utils)`

**Exports:**
- Debugging: `debug`, `pp`
- Functional: `identity`, `const`, `compose`, `curry`, `partial`
- Math: `square`, `cube`, `clamp`, `between?`
- Lists: `range`, `repeat`, `cycle`
- Higher-order: `times`, `do-times`
- Strings: `string-join`, `string-split`
- Misc: `void`, `noop`

**Usage:**
```scheme
(load "stdlib/utils.scm")
(import (zombie utils))

;; Debugging
(debug "value is:" 42)     ; Prints: DEBUG: value is: 42

;; Functional programming
(define add1 (curry + 1))
(add1 5)                    ; => 6

(define inc-and-double (compose (curry * 2) (curry + 1)))
(inc-and-double 5)          ; => 12

;; Math
(clamp 15 0 10)             ; => 10
(between? 5 0 10)           ; => #t

;; Lists
(range 0 10 2)              ; => (0 2 4 6 8)
(repeat 3 'x)               ; => (x x x)
(cycle 3 '(a b))            ; => (a b a b a b)

;; Higher-order
(times 5 (lambda (i) (* i i)))  ; => (0 1 4 9 16)
(do-times 3 (lambda (i) (display i) (display " ")))  ; Prints: 0 1 2
```

### example-app.scm - Complete Application Example

Demonstrates how to build a complete application with multiple libraries.

Shows:
- Defining multiple related libraries
- Importing between libraries
- Creating a main application module
- Running the demo

**Usage:**
```bash
./x64/Debug/REPL-test.exe < stdlib/example-app.scm
```

## Creating Your Own Libraries

### Basic Structure

```scheme
(define-library (your-library-name)
  ;; Import dependencies
  (import (scheme base))

  ;; Declare exports
  (export function1 function2)

  ;; Define implementations
  (begin
    (define (function1 x)
      (* x 2))

    (define (function2 x y)
      (+ x y))))
```

### Hierarchical Names

Organize related libraries:

```scheme
(define-library (myapp utils math)
  ...)

(define-library (myapp utils strings)
  ...)

(define-library (myapp core)
  (import (myapp utils math)
          (myapp utils strings))
  ...)
```

### Import Options

```scheme
;; Import all exports
(import (srfi 1))

;; Import only specific symbols
(import (only (srfi 1) fold filter))

;; Import all except specific symbols
(import (except (srfi 1) circular-list))

;; Import with prefix
(import (prefix (srfi 1) list:))
;; Use: (list:fold ...)

;; Import with renaming
(import (rename (srfi 1) (fold foldl) (fold-right foldr)))
;; Use: (foldl ...) instead of (fold ...)
```

## Best Practices

1. **Keep libraries focused** - One responsibility per library
2. **Document thoroughly** - Add comments and docstrings
3. **Minimize exports** - Only export public API
4. **Test separately** - Create test files for each library
5. **Use meaningful names** - Clear, descriptive names
6. **Version your libraries** - Add version info in comments

## R7RS Standard Libraries

Built into Zombie Scheme (no need to load):

- `(scheme base)` - Core language
- `(scheme read)` - Reading S-expressions
- `(scheme write)` - Writing S-expressions
- `(scheme file)` - File I/O operations

## Contributing

To contribute a library to the standard library:

1. Create your library file in `stdlib/`
2. Add comprehensive documentation
3. Include usage examples
4. Write tests in `tests/scheme/`
5. Update this README

## Directory Structure

```
stdlib/
├── README.md            # This file
├── init.scm             # Startup utilities (load first)
├── srfi-1-lists.scm     # SRFI-1 list library
├── utils.scm            # General utilities
└── example-app.scm      # Complete application example
```

## Loading at Startup

### Option 1: Redirect

```bash
./x64/Debug/REPL-test.exe < stdlib/init.scm
```

### Option 2: Shell Script

Create `start-repl.sh`:

```bash
#!/bin/bash
cat stdlib/init.scm - | ./x64/Debug/REPL-test.exe
```

### Option 3: Batch Script (Windows)

Create `start-repl.bat`:

```batch
@echo off
type stdlib\init.scm | x64\Debug\REPL-test.exe
```

## See Also

- `../LIBRARY_GUIDE.md` - Comprehensive library system guide
- `../test-libs/` - Example test libraries
- `../r5rs_tests/` - R5RS compliance tests
- `../tests/scheme/` - Scheme test files

## Resources

- [R7RS Specification](https://small.r7rs.org/)
- [SRFI Libraries](https://srfi.schemers.org/)
- [Scheme Community](https://community.schemers.org/)
