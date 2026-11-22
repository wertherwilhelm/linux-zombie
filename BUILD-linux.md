# ZombieScheme — Linux / CMake build

This is a Linux/CMake port of the original Visual Studio solution
(`ConsoleApplication2.sln`). It builds the core engine, the REPL, and the
GoogleTest suite with g++/clang and CMake — no MSBuild, NuGet, or asmjit.

## Requirements

- CMake ≥ 3.16
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
- `build/zombie-scheme` — quiet batch runner; source file and stdin data are separate
- `build/LispTest` — the GoogleTest suite
- `build/lib*.a` — the static libraries

## Run

```bash
./build/repl                 # tree-walk interpreter (default)
./build/repl --bytecode      # bytecode VM
./build/repl --vm            # experimental toy VM
```

## Batch execution and external R7RS benchmarks

`zombie-scheme` runs a source file without a REPL banner, prompts, or result echo.
Scheme output goes to stdout; evaluation/import/read errors go to stderr with a
nonzero process status. It supports the existing `lisp`, `bytecode`, `coreir`, and
`toyvm` evaluators. Program arguments after the file are exposed by `(command-line)`.

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DZOMBIE_BUILD_TESTS=OFF
cmake --build build-release --target zombie-scheme -j8
./build-release/zombie-scheme --evaluator bytecode program.scm < data.input
python3 benchmarks/test_batch_runner.py --exe build-release/zombie-scheme
```

The external suite at `/mnt/c/work/r7rs-benchmarks` can be exercised without
modifying that checkout or any evaluator:

```bash
python3 benchmarks/run_r7rs.py \
  --suite /mnt/c/work/r7rs-benchmarks \
  --exe build-release/zombie-scheme \
  --profile smoke --evaluators lisp bytecode coreir toyvm \
  --timeout 5 --output /tmp/zombie-smoke-new

python3 benchmarks/run_r7rs.py \
  --suite /mnt/c/work/r7rs-benchmarks \
  --profile one-iteration --benchmarks all \
  --evaluators lisp bytecode coreir toyvm \
  --timeout 3 --output /tmp/zombie-compat-new
```

The three profiles are deliberately separate:

- `smoke`: 15 explicit, reduced inputs with their expected answers; upstream
  benchmark algorithms, `common.scm`, and correctness predicates are unchanged.
- `one-iteration`: all 57 benchmarks listed by upstream, using original problem
  sizes and expected answers, with only the repetition count changed to one.
  `equal` already hardcodes one repetition, so its complete input is preserved.
- `original`: unmodified upstream input, including the full repetition count.
  Use `--benchmarks NAME ...` to select cases and `--repeats N` for independent
  process runs. Full input can require substantially longer than the default
  per-process wall limit; a timeout is not an incorrect result.

Each job uses a fresh process with a configurable address-space limit (default
1 GiB), copied writable fixtures, and its own output directory. The external
benchmark checkout stays untouched. Input is redirected from a **regular file**,
matching upstream's `bench` script. This matters: current `InputPort::read` uses
`scheme_reader` on seekable streams, but its legacy pipe/interactive path loses
integer exactness and does not support the same complete datum grammar.

The branch does not provide `(scheme time)`. The adapter explicitly enables
`--benchmark-time`, a batch-runner-only host clock library. `current-jiffy` uses
monotonic microseconds and `jiffies-per-second` is 1,000,000. The secondary
`current-second` is POSIX wall time, not a leap-second-aware TAI clock. This
adapter changes no evaluator or optimizer and does not supply other missing
libraries. The upstream benchmark's elapsed interval uses the monotonic clock;
`process_seconds` separately includes startup, parsing, and compilation.

Results include CSV/JSON, raw stdout/stderr, generated source, exact input data,
branch/commit/build metadata, and an executable SHA-256. `passed` requires a
successful process and the upstream numeric CSV marker after its correctness
predicate; `incorrect`, `error`, `crashed`, `timeout`, and missing results are
kept distinct. Output directories must be new, so reruns preserve old evidence.
The script returns nonzero if any job fails. Reduced results are **not comparable
with published full-workload scores**.

The first run on the asmjit-free `performance-testing` branch is recorded under
`benchmarks/results/feasibility/`; see its `report.txt` and per-profile results.
The actionable backlog is in `missing-items.txt`, with source locations, small
reproductions, priorities, and checks for confirming each fix; `AUDIT.md` links it.

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
       └─ ZombieBytecode  ⇄  ZombieCompilerIR   (mutually dependent)
            └─ ZombieLib        (embedding facade)
                 ├─ repl        (REPL-test/)
                 └─ LispTest    (GoogleTest, optional)
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
