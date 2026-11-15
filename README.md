# ZombieScheme

ZombieScheme is a multi-evaluator Scheme system implemented in C++, with a focus on
clear boundaries between compilation stages, strong runtime tooling, and an embeddable
API. It ships with a classic tree-walk interpreter, a bytecode compiler/VM, CoreIR
lowering/tooling, and an experimental toy VM, plus a GoogleTest suite and a REPL.

This is the **Linux / CMake** port of the original Visual Studio solution
(`ConsoleApplication2.sln`); see [BUILD-linux.md](BUILD-linux.md) for build details.

Key goals:
- Multiple evaluators for the same language surface.
- Easy embedding and experimentation from C++.
- A real garbage collector with a precise/explicit API and (for the runtime instance)
  conservative stack scanning.

## At a glance

- R5RS/R7RS-inspired Scheme with macros (`define-syntax` / `syntax-rules`).
- Four execution paths — tree-walk (`--lisp`), bytecode VM (`--bytecode`), the CoreIR
  pipeline (`--coreir`), and an experimental toy VM (`--vm`) — all behind one
  `IEvaluator` interface and sharing one value model, environment, reader, and
  primitive set.
- CoreIR (`ZombieCompilerIR`) is a typed intermediate representation that lowers Scheme
  to bytecode; it is selectable as an evaluator (`--coreir`) and executes on the
  bytecode VM backend.
- Mark-sweep GC with pluggable allocators; the process-wide runtime GC conservatively
  scans the C++ stack, isolated instances stay precise.
- REPL with editing and (bytecode) debugger hooks; extensive GoogleTest suite.

## Architecture

```
Scheme source
   -> Reader
   -> Expander (macros / syntax-rules, derived-form desugaring)
   -> one of:
        - Lisp interpreter (tree-walk)          [--lisp, default]
        - Bytecode compiler -> Bytecode VM      [--bytecode]
        - CoreIR lowering -> bytecode -> VM     [--coreir]
        - Toy VM (experimental)                 [--vm]
```

`eval` (the `IEvaluator` entry) is the single public evaluation call; each evaluator
implements it over the shared runtime.

## Repository layout

```
CMakeLists.txt         # Linux/CMake build
ZombieRuntime/         # Runtime types, reader, primitives, tree-walk evaluator, GC integration
GarbageCollector/      # Mark-sweep GC, allocators, gc_ptr/gc_root, tracer
ZombieBytecode/        # Bytecode compiler + VM
ZombieCompilerIR/      # CoreIR lowering, verifier, bytecode translation
ZombieLib/             # Engine orchestration and embedding facade (scheme_engine)
REPL-test/             # REPL executable
LispTest/              # GoogleTest suite
stdlib/                # Scheme standard library
test-libs/             # Sample libraries used by tests
```

## Build

Requires CMake and a C++20 compiler (g++/clang). See [BUILD-linux.md](BUILD-linux.md).

```bash
cmake -S . -B build
cmake --build build -j
```

Binaries land in `build/` (`repl`, `LispTest`). An AddressSanitizer build can be
configured in a separate directory (e.g. `build-asan`).

## Run

```bash
./build/repl              # tree-walk evaluator (default)
./build/repl --bytecode   # bytecode compiler + VM
./build/repl --coreir     # CoreIR lowering -> bytecode -> VM
./build/repl --vm         # toy VM (experimental)
```

## Tests

```bash
./build/LispTest                                   # full suite
./build/LispTest --gtest_filter='BytecodeVMTest.*' # focused
```

## Basic Scheme usage

```scheme
(define (square x) (* x x))
(square 7)                       ; => 49

(define-syntax swap!
  (syntax-rules ()
    ((_ a b) (let ((tmp a)) (set! a b) (set! b tmp)))))

(define x 1) (define y 2)
(swap! x y)
(list x y)                       ; => (2 1)
```

## Embedding (C++)

Minimal embedding via `scheme_engine` (see `ZombieLib/scheme_engine.h`):

```cpp
#include "scheme_engine.h"

scheme_engine engine(EvaluatorType::BYTECODE);
Object* result = engine.eval_string("(+ 2 3)");
```

## Documentation

- [BUILD-linux.md](BUILD-linux.md) — build instructions.
- [AUDIT.md](AUDIT.md) — ongoing code audit, fix log, and known-issue tracker.
- [stdlib/STDLIB_README.md](stdlib/STDLIB_README.md) — standard library.
- [GarbageCollector/GC_ARCHITECTURE_AND_PROGRAMMER_GUIDE.md](GarbageCollector/GC_ARCHITECTURE_AND_PROGRAMMER_GUIDE.md)
  and [GarbageCollector/TRACER_MIGRATION_GUIDE.md](GarbageCollector/TRACER_MIGRATION_GUIDE.md) — GC internals.

## Troubleshooting

- "Exec format error" under WSL: run `wsl --shutdown`, reopen the terminal, and rebuild.

## License

See repository documentation for licensing and third-party notices.
