# ZombieScheme — Code Audit (missing / unimplemented / outstanding)

Date: 2026-11-13. Scope: the ported Linux tree (`GarbageCollector`,
`ZombieRuntime`, `ZombieBytecode`, `ZombieCompilerIR`, `ZombieLib`, `REPL-test`).
Findings were verified empirically against the built `repl`/`LispTest`, not taken
from the (stale) docs. Severity is my assessment for someone intending to use
this as a working Scheme.

> Note on docs: `docs/STATUS.md`, `R5RS_COMPLIANCE_2025.md`, etc. are out of date.
> E.g. the R5RS doc claims "no rationals/complex", but both work
> (`(+ 1/2 1/3)` → `5/6`, `(* 2+3i 1-1i)` → `5+1i`). Trust the code, not the docs.

---

## Fix log (2026-11-13)

- [x] **§1.2 list printing** — `ConsCell::print` rewritten to walk the spine and
      emit `(a b c)` / `(a b . c)`. Verified in REPL; 1269 tests still pass.
- [x] **§1.1 `and` / `or` in tree-walk** — added to both `eval_list` and the
      `eval_step_list` trampoline (TCO-correct, R5RS short-circuit). Re-enabled and
      expanded the `BooleanTests` (`AndWithBooleans`, `OrWithBooleans`, identities,
      return-value, short-circuit) — 7 tests, all pass.
- [x] **§1.3 `call-with-current-continuation`** — fixed the `…-Continuation`
      (capital C) typo in primitive registration, r7rs_stdlib export, toy VM, and
      the bytecode compiler. Long name now returns 42 in both `--lisp` and
      `--bytecode`.
- [x] **§3.2 `deallocate()` stale tracking (double-free)** — made
      `IGarbageCollector::deallocate()` a safe no-op (correct tracing-GC
      semantics; no production caller existed). Added regression test
      `GCDeallocateSafety.DeallocateThenCollectDoesNotDoubleFree` using an
      isolated `gc_manager`.
- [x] **§3.2 generational minor-GC copy alignment** — `copy_object_cheney`
      advanced `free` by raw `total_size`, drifting block starts off-alignment
      (misaligned `gc_header` access = UB). Now rounds both the `free` advance and
      the Cheney `scan` advance up to `alignof(std::max_align_t)` in lockstep.
      (Objects over-aligned beyond `max_align_t` remain unsupported by the moving
      young gen — none exist in the runtime.)

- [x] **§3.2 CRITICAL GCPolicy allocates outside GC tracking** — decided to
      support it. `GCPolicy::allocate<T>` now delegates to
      `get_current_gc()->allocate<T>()` (installs the gc_header + registers via
      `on_allocate`), `deallocate` is a no-op, and the dead `gc::current()`
      forward-decl (wrong namespace + return type, couldn't link) was replaced with
      the real `gc_context`/`get_current_gc()` API. `allocate` now static_asserts
      `T : gc_object`. Added `GCPolicyIntegration` tests (tracked allocation,
      collection of unreachable objects, no-op deallocate).

- [x] **§3.1 template-GC "pure virtual method called" crash** — *misdiagnosed in
      the original audit* (it is **not** a dangling vtable in the mark worklist).
      Real cause: member-destruction order in `gc_manager_template.h`. The primary
      `gc_manager_t` template declared `m_strategy` before `m_alloc`, so `m_alloc`
      was destroyed *first*. `~MarkSweepStrategy::shutdown()` then reclaimed any
      surviving object via `gc_box::deallocate(m_alloc, …)`, whose `alloc.deallocate()`
      is a **virtual** call — dispatched on the already-destroyed allocator, landing
      on the pure-virtual `IAllocator::deallocate`. Only surfaced when an object
      survived to shutdown (`TemplateGCWithRootsPreventsCollection`), and only at
      `-O0` (at `-O1`/ASan the call is devirtualized, which is why ASan "passed").
      Fixed by declaring `m_alloc` *before* `m_strategy` (matching the
      `GenerationalGCStrategy` specialization, which already did). All 9
      `GCAllocatorTest.*` now pass at `-O0` and under ASan and are **re-enabled in
      ctest** (ctest is now 1289/1289).

- [x] **ASan: stack-use-after-scope in production `gc_manager` mark()** — found by
      building `-fsanitize=address` and running the full suite. Test bug in
      `ExtraTests.cpp` `GCRootsDoNotLeakAfterRemoval`: it registered
      `add_root(&guard)` where `guard` is a **loop-local**, so `m_persistent_roots`
      held pointers into a dead stack slot that `mark()` later dereferenced (and the
      cleanup loop passed a different address, so the roots were never removed —
      the "flaky under full suite" the author noted). Fixed to register the stable
      vector-element slot (`&guards.back()`, with `reserve`). ASan is now clean
      across all 1289 tests.

Remaining items below are unaddressed. Not done (your call): pool-allocator
alignment, `release()` semantics, dual mark bits; §2 toy-VM / CoreIR; §4
embedding stubs.

- [x] **§5 CppObjectTests GC-suite leaks** — converted every raw `new <gc-type>`
      (`a_double`/`a_string`/`a_symbol`/`atom<>`/`ConsCell`) in
      `LispTest/CppObjectTests.cpp` to `gc_new<T>(...)`, matching the fixture idiom
      (objects bypassing the GC via global `new` was simply wrong for GC-managed
      types). The `*GC*` suite (91 tests) is now **leak-clean under ASan** (was 6
      leaks / 240 B). All 457 object tests still pass at `-O0`; 516 pass under ASan.
      NOTE: the *whole* test binary is still not globally leak-clean — `clone()`
      results (e.g. `CppObjectTests.cpp:16`) and other files (`NumericTowerTests`,
      `MissingR5RSTests`) also allocate unrooted GC objects that LSan flags at exit.
      That is a larger, separate item (see §5) and overlaps the P1 collection bugs
      below — forcing a teardown `collect()` to reap test garbage would exercise
      exactly those paths, so defer until they're fixed.

---

## 0. TODO — revisit: two P1 GC bugs (allocation can return freed objects)

Flagged for evaluation (not yet verified/fixed in this tree):

### 0.1 [P1] Allocation can return a freed object (collect-before-root)
`gc_manager_template.h:232` triggers a collection **before** the newly allocated
object is rooted or returned. With the GC threshold set to zero, the object's
destructor runs *inside* `allocate()`, which then hands back a dangling pointer.
The production collector has the same pattern at `gc_manager.cpp:103`.
Fix direction: protect the in-flight allocation across the collection (temporary
root / defer registration), **or** collect *before* constructing the new object.

### 0.2 [P1] Promoted objects' children skipped during minor collection
`generational_gc_strategy.cpp:527` returns a promoted object without scanning its
children or adding it to the scan queue. Repro: a parent survives two collections,
then gets a new child; on the third collection the parent is promoted while its
child pointer still lives in discarded from-space (dangling).
Fix direction: scan promoted objects and forward their children before completing
the collection (don't short-circuit the promotion path out of the Cheney scan).

---

## 1. Correctness bugs in active code paths

### 1.1 HIGH — `and` / `or` are unbound in the default tree-walk evaluator  ✅ FIXED
`(and 1 2)` and `(or #f 5)` both fail with **"Unbound variable: and/or"** in the
default `--lisp` evaluator. These are core R5RS special forms.

- Root cause: `Lisp::new_eval` (ZombieRuntime/lisp.cpp) dispatches `if`, `cond`,
  `when`, `unless`, `case`, `do`, `let`/`let*`/`letrec`/named-`let`, `begin`,
  quasiquote, and user `define-syntax` macros — but **not `and`/`or`**. The
  `Expander` class *does* handle `and`/`or` (expander.cpp:297,301) but `new_eval`
  never invokes the standalone expander (only `transformer->expand` for user
  macros).
- The `--bytecode` evaluator handles them (compiler.cpp:210,214), so this is an
  **evaluator inconsistency**: the same program behaves differently per backend.
- The unit tests for this are **commented out** — `BooleanTests.cpp` lines
  113–141 (`AndWithBooleans`, `OrWithBooleans` inside `/* … */`), so the gap is
  invisible to CI. The author clearly knew.
- Fix: add `and`/`or` cases to `new_eval` (expand to nested `if`), or route
  top-level forms through `Expander` before eval. Then un-comment the tests.

### 1.2 HIGH — list printing is wrong (both `display` and `write`)  ✅ FIXED
Proper lists print in fully-dotted form:
`(display '(1 2 3))` → `(1 . (2 . (3 . ())))` (expected `(1 2 3)`); same for
`write`. The cons printer never special-cases the "rest is a pair" case to use
list notation. User-visible on every list result in the REPL.
- Location: the `print`/`write` path for `ConsCell` (ZombieRuntime/cons_cell +
  object print). Fix the printer to emit `(a b c)` / `(a b . c)` form.

### 1.3 MEDIUM — `call-with-current-continuation` (long name) is broken everywhere  ✅ FIXED
Only the short alias `call/cc` works.
- Tree-walk: long name is unbound (`call/cc` works).
- Bytecode: typo — `compiler.cpp:218` and `:538` test for
  `"call-with-current-Continuation"` (capital **C**), so the real R7RS name
  `call-with-current-continuation` never matches and falls through to an
  (empty) unbound-variable error. One-character fix.

---

## 2. Evaluators

### 2.1 Toy VM (`--vm`) — continuations broken; experimental
- `(call/cc (lambda (k) (k 7)))` returns garbage (`(k . (7 . ()))`) instead of 7.
- `toy_vm.cpp:504` / `:620` throw "call/cc / dynamic-wind: Continuation support
  not enabled". The toy VM is explicitly experimental; treat as demo-only.

### 2.2 CoreIR (`ZombieCompilerIR`) — incomplete by design
Per `docs/COREIR_ROADMAP.md` it is a "proof-of-concept"; confirmed in code:
- `coreir.h:144` — "Lambda execution is TODO for Phase 1".
- `scheme_to_coreir.cpp:789` — no `SetLocal`/local-variable lowering yet
  ("once we track lexical scope").
- `scheme_to_coreir.cpp:1449` — `throw … "Cannot inline stmt chain with If
  statement (clone not implemented)"`.
- No JIT backend (the roadmap's headline goal is unstarted).
It is used as tooling/translation infra, not a user-facing evaluator — the engine
exposes no CoreIR evaluator (README notes `--coreir` was removed).

---

## 3. Garbage collector

### 3.1 HIGH — experimental template GC crashes on Linux ("pure virtual method called")  ✅ FIXED
The 9 `GCAllocatorTest.*` tests exercise `gc_manager_t<Strategy, Allocator>`.
~~Several crash: `MarkSweepStrategy::mark()` calls `trace_references()` on an
object whose vtable is already freed — a dangling pointer in the mark worklist.~~
**That diagnosis was wrong.** The actual bug was member-destruction order in
`gc_manager_template.h` (allocator destroyed before the strategy → `shutdown()`
reclaims through a dead allocator → virtual `deallocate()` hits the pure-virtual
`IAllocator`). See the fix-log entry above. Fixed; all 9 now pass at `-O0` and
under ASan and are re-enabled in ctest.

### 3.2 Documented GC issues (from `docs/GARBAGECOLLECTOR_ISSUES.md`, still present)
- **CRITICAL** `MemoryManagement/policies/GCPolicy.h` allocates outside
  `gc_box` tracking → objects invisible to GC / double-free risk.  ✅ FIXED
- **HIGH** `IGarbageCollector::deallocate` calls `reclaim()` without removing the
  object from `all_objects_` → re-reclaimed on next sweep.  ✅ FIXED
- **HIGH** generational minor-GC copy advances `free` by `total_size` without
  re-aligning → UB for over-aligned types.  ✅ FIXED
- **MEDIUM** `PoolAllocator` (gc_allocator.h) ignores alignment requests.
- **MEDIUM** `gc_root/gc_ptr::release()` unroots shared root entries.
- **LOW** two mark bits (`gc_header::marked` vs `gc_object::_marked`) can diverge.

### 3.3 By-design hard stops
- `GenerationalGCStrategy::add_root/remove_root` call `std::abort()`
  unconditionally (the slot API is incompatible with a moving collector). So the
  generational strategy only works via `register_persistent_root`/`gc_root<T>`.

---

## 4. Embedding / interop gaps
- `scheme_engine(use_isolated_gc=true)` throws "Isolated GC not yet supported"
  (scheme_engine.cpp:40,85) — only the shared global GC is available.
- `NativeMirror::invoke` throws "not yet implemented" (mirror.cpp:127;
  mirror.h:119 "TODO: variadic argument unpacking"). Native-side mirror calls
  across the reified boundary are stubbed.
- `embedding_api.cpp:73,138` — "TODO: Hook into Macro expansion system" (embedded
  compile path doesn't run macros).
- `ZombieLib/ZombieLib.cpp` is still the VS template stub ("example library
  function").

---

## 5. Test-suite coverage gaps
Current: **1289/1289 pass under ctest; no memory *errors* under
`-fsanitize=address`.** The GC suite (`*GC*` filter, 91 tests) is fully clean
under ASan — no errors and no leaks — at both `-O0` and ASan builds (re-verified
2026-11-13 after rebuilding the stale ASan `LispTest`).

> **LSan caveat (2026-11-13):** "clean under ASan" refers to memory *errors*
> (UAF / overflow / the use-after-scope bug fixed above), which remain clean. The
> `*GC*` suite is now also **leak-clean** (the raw-`new` atoms in `CppObjectTests`
> were converted to `gc_new<T>` — see §0 fix log). The **whole test binary is not
> yet globally leak-clean**, though: LSan still reports direct leaks of unrooted GC
> objects created by `clone()` (e.g. `CppObjectTests.cpp:16`, atom.h's
> `clone()` → `gc_new<…>`) and by other suites (`NumericTowerTests`,
> `MissingR5RSTests`). These are **test-only** (the global test GC never sweeps
> unrooted garbage before process exit). Remaining options to get the full binary
> leak-clean: (a) a `TearDown` that forces `gc->collect()` to reap per-test garbage
> — but this exercises the §0 P1 collection paths, so do it *after* those are fixed;
> (b) run with `ASAN_OPTIONS=detect_leaks=0`; or (c) add an LSan suppression. Left
> for the user to decide.

Older suppressions, now partly resolved:
- `and`/`or` tests commented out (§1.1).  ✅ re-enabled
- `GCBoxTests.cpp` excluded from build (stale `add_root(object*)` API).
- 9 `GCAllocatorTest.*` excluded from ctest (§3.1).  ✅ fixed and re-enabled
- 4 `exit`/`emergency-exit` tests commented out (would terminate the process —
  legitimate; could be re-enabled via `EXPECT_EXIT`/death tests).
- The file-IO suites share fixed scratch filenames and are only safe under ctest
  because of the `RESOURCE_LOCK` added during the port (they'd race otherwise).

---

## 6. Minor / cleanup TODOs found in code
- `generational_gc_strategy.cpp:196-197` — stats (`objects_collected`,
  `bytes_reclaimed`) hard-coded to 0.
- `gc_allocator.h:157` — pool allocator "Support multiple size buckets".
- `constants.cpp:91` — native/bytecode/dream skins not registered.
- `procedure.h:129` — typed call returns a dummy Procedure on type mismatch
  instead of erroring.
- `runtime_context.cpp:154` — needs a lookup-only constants function.
- `toy_vm.cpp:950` — dynamic-wind marker reuses `expr` field ("hacky but works").

---

## Suggested priority order
1. §1.2 list printing (every REPL user hits it).
2. §1.1 `and`/`or` in tree-walk + un-comment tests (core language, silent gap).
3. §1.3 `call-with-current-continuation` long-name (one-char typo + tree-walk alias).
4. §3.2 GC correctness items (CRITICAL/HIGH) if the template/generational/policy
   paths are meant for production; otherwise mark them experimental and gate them.
5. §3.1 fix or formally quarantine the template GC.
6. Decide scope for CoreIR/toy VM/mirrors/isolated GC — finish or label experimental.
