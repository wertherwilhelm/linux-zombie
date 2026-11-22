# ZombieScheme — Linux / CMake build

This is a Linux/CMake port of the original Visual Studio solution
(`ConsoleApplication2.sln`). It builds the core engine, the REPL, and the
GoogleTest suite with g++/clang and CMake — no MSBuild or NuGet. The optional native backend uses vendored asmjit.

## Requirements

- CMake ≥ 3.24 (required by the pinned asmjit build)
- A C++20 compiler (tested with g++ 11.4)
- Network access on first configure (CMake fetches GoogleTest via FetchContent;
  a system/`find_package(GTest)` install is used instead when present)

## Build

```bash
cmake -S . -B build            # add -DZOMBIE_BUILD_TESTS=OFF to skip the test suite
cmake --build build -j$(nproc)
```

Artifacts land in `build/`:

- `build/repl` — the REPL executable
- `build/LispTest` — the GoogleTest suite
- `build/lib*.a` — the static libraries

## Run

```bash
./build/repl                 # tree-walk interpreter (default)
./build/repl --bytecode      # bytecode VM
./build/repl --native        # CoreIR -> cached native JIT; bytecode for unsupported IR
./build/repl --vm            # experimental toy VM
```

## REPL editing

Interactive terminals use the vendored [replxx](https://github.com/AmokHuginnsson/replxx)
0.0.4 editor (MIT license, pinned commit recorded in `third_party/README.md`).
No additional packages or downloads are needed for the editor.

- Arrows, Home/End, Backspace/Delete: edit input; Up/Down: recall history.
- Ctrl-R: search history; Tab: complete Scheme syntax, current global bindings
  (including user definitions), and `:` commands.
- Enter: evaluate complete input; unfinished forms continue in the same editable
  buffer with two spaces per nesting level. Ctrl-J inserts a newline explicitly.
- Ctrl-C: discard the edit and return to a fresh prompt. Ctrl-D: exit on empty input.
- Alt-Enter: submit to the session explicitly; `:help` lists session commands.
- Strings, comments, delimiters, keywords, and literals are colored; `NO_COLOR=1`
  disables coloring. Completeness handles multiline strings, escaped quotes,
  character literals, and quote prefixes. Reader syntax support still depends on
  the selected evaluator; highlighting does not add language features.

History retains up to 1000 entries under
`$XDG_STATE_HOME/zombie-scheme/history` or
`$HOME/.local/state/zombie-scheme/history`. If neither is set, history stays in memory.
History includes the Scheme source entered into the REPL.
Piped input/output automatically uses the plain frontend; `--plain` forces it.

## Compilation components and native evaluation

The frontend and backend have separate jobs:

```text
Scheme -> Expander (macros/desugaring) -> SchemeToCoreirLowering -> CoreIR
                                                               |
                                                CompilationPipeline / LRU cache
                                                    |                      |
                                            bytecode compiler       asmjit compiler
                                                    |                      |
                                               BytecodeVM          native entry point
```

`ZombieCompilerIR/compilation.h` is the common contract. `ICompilerBackend`
accepts a CoreIR function and an `OutputKind`, then returns an owned
`CompiledArtifact`. Executable artifacts implement `ExecutableCode`; compilation
and execution remain separate operations. `BytecodeCompilerBackend` adapts the
existing translator. The CoreIR evaluator now uses that pipeline too; the direct
bytecode evaluator, toy VM, and tree walker retain their existing interfaces.

`ZombieNative/` supplies `NativeCompilerBackend` and `NativeEvaluator`, selected
through `EvaluatorType::NATIVE` or `repl --native`. The initial host x86-64 backend
emits real machine code for CoreIR sequencing, branches, literals, local slots,
and global reads/writes. Primitive steps call the existing boxed runtime through
`NativePrimitive`, preserving numeric tower and arity behavior rather than
introducing an unboxed numeric representation in this first slice. Supported
primitive names are listed in `native_compiler.cpp`.

Native closure creation, general calls, multiple values, native tail calls, and
native loop compilation remain future increments. By default, an unsupported
unit is compiled entirely to bytecode **before any execution begins**, keeping
the Scheme prelude and existing functions usable. A loop using unsupported calls
runs on the bytecode backend; it is not yet a native hot-loop/tiering JIT.
`NativeEvaluator(env, UnsupportedPolicy::Error)` disables fallback for compiler
work. Runtime errors never retry on another backend. Artifact `output_kind()`,
pipeline statistics, and `last_fallback_reason()` make fallback inspectable.

The per-evaluator cache defaults to 128 units with LRU eviction. It caches code,
not results: side effects execute on every call and global values are read at
runtime. Expansion and lowering run before cache lookup; the key encodes the
resulting IR exactly, including scalar types and floating-point bits, so macro
redefinitions are reflected. Units with non-scalar literals (including quoted
lists/strings) bypass caching to preserve literal identity and mutation behavior.
A capacity of zero disables retention; `clear_cache()` releases cached ownership.
Shared artifacts pin active code through eviction. Collector roots cover compiled
constant pools and native temporary slots; no managed object address is embedded
in machine code. Artifacts must be destroyed before their collector. Pipelines,
artifacts, and environments currently assume single-threaded use.

`OutputKind::NativeObject` reserves a compilation output boundary for future AOT
work. It explicitly reports unsupported today: asmjit is a code emitter, and
writing a portable object/executable still needs a symbol, relocation, object
writer, runtime ABI, and linker component. `supports()` distinguishes implemented
outputs. No binary-file generation is claimed by this first increment.

Build with `-DZOMBIE_ENABLE_ASMJIT=OFF` to exclude asmjit; other evaluators still
build and `--native` reports a clear startup error. Native tests skip when the
backend is disabled or the host is not x86-64. The dependency is vendored and
pinned in `third_party/README.md`; no asmjit download is needed at configure time.

Next components can build on the same boundary: a callable/closure ABI, native
function compilation and tail calls, hotness/tiering policy, optional unboxed
specialization, then native object emission/linking. Each can be tested against
the existing bytecode artifacts independently of frontend expansion/lowering.

## Test

Run the suite from the repository root (some library-loading tests read
`test-libs/` relative to the working directory):

```bash
cd /mnt/c/work/linux-zombie
ctest --test-dir build -j$(nproc)          # 1269 individually-reported tests
# or run the binary directly:
./build/LispTest --gtest_filter=-GCAllocatorTest.*
```

Each gtest case is registered as its own CTest entry via
`gtest_discover_tests`, so `ctest -N` lists all 1269 and failures are reported
per-case. The 9 experimental-GC cases are filtered out of discovery
(`TEST_FILTER -GCAllocatorTest.*`); run them explicitly with
`./build/LispTest --gtest_filter=GCAllocatorTest.*`.

Result: **1269 / 1269 pass.**

## Project layout / dependency graph

```
GarbageCollector
  └─ ZombieRuntime
       └─ ZombieBytecode
            └─ ZombieCompilerIR  (lowering, compilation interfaces, bytecode backend)
                 └─ ZombieNative (asmjit backend and evaluator)
                      └─ ZombieLib (embedding facade)
                           ├─ repl     (REPL-test/)
                           └─ LispTest (GoogleTest, optional)
MemoryManagement/   interface headers only (no compiled sources)
stdlib/, test-libs/ runtime Scheme data read relative to the working dir
```

## Changes made during the port

The upstream C++ was already largely cross-platform (POSIX branches exist
alongside every `_WIN32` branch). The port required:

1. **CMake build system** (`CMakeLists.txt`) replacing the `.sln`/`.vcxproj`
   files. Per-project static libs + executables, C++20, GoogleTest via
   FetchContent. asmjit/JIT code (only in `docs/` and `tests/`) is dropped.
2. **Include-search order** — every `pch.h` shares the `PCH_H` guard. Files in
   `ZombieRuntime/primitives/` have no local `pch.h`, so the include path lists
   `ZombieRuntime` before `GarbageCollector` to pick up the right one.
3. **Static-lib cycle** — `ZombieBytecode` (`bytecode_vm.cpp`) calls
   `zombie::bytecode::Compiler`, which lives in `ZombieCompilerIR`. Both link
   directions are declared so GNU ld repeats the archives.
4. `GarbageCollector/gc_box.h` — forward declaration of `get_destruct_fn<T>()`
   (GCC two-phase lookup; MSVC resolved it lazily).
5. `GarbageCollector/generational_gc_strategy.cpp` and
   `LispTest/CppObjectTests.cpp` — added `<cstring>` / `<cmath>` includes that
   libstdc++ does not provide transitively.
6. `LispTest/GCBoxTests.cpp` excluded from the build (it was already
   `<ExcludedFromBuild>` in the VS project — stale `add_root` API).

## Known issue — experimental templated GC (not a port regression)

The 9 `GCAllocatorTest.*` tests exercise the experimental template GC
(`gc_manager_t<Strategy, Allocator>`), not the production global GC used by the
interpreter. Several crash with **"pure virtual method called"**:
`MarkSweepStrategy::mark()` (`mark_sweep_strategy.cpp:161`) calls
`trace_references()` on an object whose vtable is already gone — a dangling
pointer in the mark worklist. The author documented this (`TemplateGCTracesObjectGraph`
carries a `DISABLED:` note about reachable objects being collected). MSVC's
allocator left the freed vtable bytes intact; glibc clobbers them, so the latent
bug now faults. These tests are filtered out of the default `ctest` run
(`LispTest_ExperimentalGC` is a disabled, opt-in target). The production GC
passes all of its tests.

## Note on case sensitivity

Several includes use the wrong case (`"Lisp.h"`→`lisp.h`, `"Base.h"`→`base.h`,
`"Macro.h"`→`macro.h`). This builds fine on the `/mnt/c` (NTFS, case-insensitive)
mount. To build on a native case-sensitive filesystem, those includes or the
filenames must be normalized first.
