# ZombieScheme

ZombieScheme is a multi-evaluator Scheme system implemented in C++ with a focus on clear boundaries between compilation stages, strong runtime tooling, and embeddable APIs. It ships with a classic tree-walk interpreter, a bytecode compiler/VM, CoreIR lowering/tooling, and an experimental toy VM, plus a robust test suite and REPL.

Key goals:
- Multiple evaluators for the same language surface.
- Explicit, inspectable boundary metadata (value ledger + mirrors).
- Easy embedding and experimentation from C++.

## At A Glance

- R5RS/R7RS-inspired Scheme with macros (`define-syntax`, `syntax-rules`).
- Multiple execution paths: Lisp (tree-walk), Bytecode VM, and Toy VM, with CoreIR used as compiler/tooling infrastructure.
- GC-backed runtime with value ledger and provenance tooling.
- REPL with smart editing and debugger hooks (bytecode).
- Extensive GoogleTest suite.

## Architecture

@startuml
title Scheme Pipeline + Explicit Interop Boundary

skinparam shadowing false
skinparam monochrome true
skinparam componentStyle rectangle
skinparam ArrowColor Black
skinparam DefaultTextAlignment center

' ---------------------------
' Front-end pipeline
' ---------------------------
rectangle "Scheme source" as SRC

component "Reader" as READER
component "Expander\n(macros, syntax rules)" as EXPANDER

SRC --> READER
READER --> EXPANDER

' ---------------------------
' Evaluator selection
' ---------------------------
rectangle "Choose evaluator" as CHOOSE

EXPANDER --> CHOOSE

package "Evaluators" as EVALS {
  component "Lisp interpreter\n(tree-walk)" as TREEWALK
  component "Bytecode compiler" as BCC
  component "Bytecode VM" as BCVM
  component "CoreIR lowering/tooling" as CORELOWER
  component "Toy VM\n(experimental)" as TOYVM
}

CHOOSE --> TREEWALK

CHOOSE --> BCC
BCC --> BCVM

CHOOSE --> CORELOWER

CHOOSE --> TOYVM

' ---------------------------
' Interop boundary (reified)
' ---------------------------
package "Interop boundary (explicitly reified)" as INTEROP {
  component "C++ host" as CPP
  component "Scheme values" as SVAL

  database "Value ledger" as LEDGER
  component "Mirrors\n(optional, inspectable boundary metadata)" as MIRRORS

  CPP <--> SVAL : marshal / unmarshal
  CPP --> LEDGER : register / track
  SVAL --> LEDGER : identity / lifetime
  LEDGER --> MIRRORS : reflect / inspect
  MIRRORS --> CPP : debug / tooling
}

' (Optional) show boundary touchpoints in the pipeline
TREEWALK ..> INTEROP : FFI calls
BCVM ..> INTEROP : native stubs
CORELOWER ..> INTEROP : host ops
TOYVM ..> INTEROP : experimental FFI

@enduml

## Repository Layout

```
ConsoleApplication2.sln
ZombieLib/             # Engine orchestration and embedding facade
ZombieRuntime/         # Runtime types, primitives, GC integration
ZombieBytecode/        # Bytecode compiler + VM
ZombieCompilerIR/      # CoreIR lowering, verifier, bytecode translation
REPL-test/             # REPL executable
LispTest/              # GoogleTest suite
zombie-experimental-app/ # Embedding + interop experiments
test_*.scm             # Sample Scheme programs
docs/                  # Technical docs and deep dives
```

Build outputs land in `x64/Debug/` by default (`LispTest.exe`, `REPL-test.exe`, `ZombieLib.lib`, and dependent static libraries).

## Build

Debug build (core library + tests):

```bash
"/mnt/c/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" \
  ConsoleApplication2.sln /p:Configuration=Debug /p:Platform=x64 /t:ZombieLib,LispTest /maxcpucount:1
```

## Run

REPL (default tree-walk evaluator):

```bash
./x64/Debug/REPL-test.exe
```

REPL using bytecode VM:

```bash
./x64/Debug/REPL-test.exe --bytecode
```

Evaluator options:

- `--lisp` (default)
- `--bytecode`
- `--vm` (toy VM, experimental)

Note: some CLI help text still mentions `--coreir`, but the current `scheme_engine`
does not expose a CoreIR evaluator. See [STATUS.md](docs/STATUS.md).

## Tests

Run the full suite:

```bash
./x64/Debug/LispTest.exe
```

Focused bytecode/dynamic-wind tests:

```bash
./x64/Debug/LispTest.exe --gtest_filter="BytecodeVMTest.DynamicWind*"
```

## Basic Scheme Usage

Example (REPL):

```scheme
(define (square x) (* x x))
(square 7)

(define-syntax when
  (syntax-rules ()
    ((_ test body ...)
     (if test (begin body ...) '()))))

(when #t (display "hi\n"))
```

## Embedding (C++)

Minimal embedding sketch using `scheme_engine`:

```cpp
scheme_engine engine(EvaluatorType::BYTECODE);
object* result = static_cast<object*>(engine.eval_string("(+ 2 3)"));
std::cout << engine.print_value(result) << "\n";
```

For a richer example (exporting C++ callables, value ledger, mirrors, typed calls),
see `zombie-experimental-app/`.

## Boundary Reification (Value Ledger + Mirrors)

ZombieScheme can attach provenance metadata to values and expose mirrors for
cross-boundary invocation. This keeps the boundary visible and manipulable
without making normal calls feel “special.”

Helpful docs:
- [VALUE_LEDGER_PRIMITIVES_GUIDE.md](docs/VALUE_LEDGER_PRIMITIVES_GUIDE.md)
- [PRIMITIVES_QUICK_REF.md](docs/PRIMITIVES_QUICK_REF.md)

## Documentation Map

Start here:
- [STATUS.md](docs/STATUS.md) (current resync status and known blockers)
- [QUICK_START_AFTER_REBOOT.md](docs/QUICK_START_AFTER_REBOOT.md) (build + test quickstart)
- [QUICK_TEST_GUIDE.md](docs/QUICK_TEST_GUIDE.md) (focused testing flow)
- [REPL-test/](REPL-test/) for REPL details and options
- [ZOMBIE_LIBRARIES.md](docs/ZOMBIE_LIBRARIES.md) (library layers and responsibilities)
- [docs/CODING_STYLE_GUIDE.md](docs/CODING_STYLE_GUIDE.md) (coding conventions)
- [docs/DOCUMENTATION_STYLE.md](docs/DOCUMENTATION_STYLE.md) (Doxygen and docs conventions)

Deep dives:
- [BYTECODE_VM_SUMMARY.md](docs/BYTECODE_VM_SUMMARY.md)
- [COREIR_ROADMAP.md](docs/COREIR_ROADMAP.md)
- [R5RS_COMPLIANCE_2025.md](docs/R5RS_COMPLIANCE_2025.md)
- [R7RS_FEATURES_COMPLETE.md](docs/R7RS_FEATURES_COMPLETE.md)
- [docs/GC/GC_README.md](docs/GC/GC_README.md) (garbage collector internals)

## Troubleshooting

- If you see “Exec format error” under WSL, run `wsl --shutdown` and reopen your terminal.
- If a build fails after toolchain updates, clean and rebuild `ZombieLib` first.

## License

See repository documentation for licensing and third-party notices.
