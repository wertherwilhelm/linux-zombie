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

## Evaluator architecture audit (2026-11-13)

Four execution paths, one shared value/runtime core.

**Inventory**
| Path | Module / entry | IEvaluator? | Role |
|---|---|---|---|
| Tree-walk `Lisp` | `ZombieRuntime/lisp.cpp` (1847) `new_eval`/`eval_step` | yes | default `--lisp`; reference evaluator |
| Bytecode | `ZombieBytecode/bytecode_vm.cpp` (655) + `Compiler` in **`ZombieCompilerIR/compiler.cpp`** (920) | yes (`BytecodeVM`) | `--bytecode` |
| Toy VM | `ZombieRuntime/toy_vm.cpp` (984) | yes | `--vm`, experimental |
| CoreIR | `ZombieCompilerIR/` scheme_to_coreir(1455)+coreir+verifier+coreir_to_bytecode (~4.4k) | **no** (compiler pipeline) | Scheme→CoreIR→bytecode; not wired to REPL/tests |

**What's shared (good isolation):** one Object/atom/cons value model, one `Environment`,
one reader, one primitive set. `IEvaluator` (i_evaluator.h) is a clean 2-method seam
(`eval`, `get_environment`). `scheme_engine` owns the evaluator via
`create_evaluator(EvaluatorType)` + `owns_evaluator_`, and can take an injected evaluator —
good ownership/isolation. No evaluator global-state leaks remain (the ledger's `g_lisp` was
deleted). Runtime evaluators don't call each other.

**Layering problems**
1. **Circular module dependency:** `ZombieBytecode → ZombieCompilerIR` (CMakeLists:87) *and*
   `ZombieCompilerIR → ZombieBytecode` (:81). The bytecode `Compiler` lives in
   `ZombieCompilerIR` (so the VM depends on it), while `coreir_to_bytecode` emits ZombieBytecode
   (so CoreIR depends back). The two libs are one tangled unit, not layers.
2. **Misplaced compiler:** the plain bytecode `Compiler` sits in `ZombieCompilerIR`, which also
   holds CoreIR — the module name/placement conflates "bytecode compiler" with "CoreIR pipeline".
3. **Front-end applied asymmetrically (semantic-drift risk):** bytecode (compiler.cpp:35,40) and
   toy VM run the shared `Expander` (expander.cpp — desugars let/let*/letrec/case/do/when/unless/
   and/or/quasiquote/cond + user `define-syntax`) before executing core forms. The **tree-walk
   does NOT** run the Expander front-end; `new_eval` reimplements the derived forms inline and
   only calls the expander for *user* macros (lisp.cpp:991/1671). So derived-form semantics exist
   in two places and can drift — exactly the earlier `and`/`or` and `call/cc` per-evaluator bugs.

**Completeness**
- Tree-walk: fullest R5/R7 — adds `define-library`, `import`, `parameterize`, `guard`, `delay`,
  `case-lambda`, `cond-expand`, full macros. The reference.
- Bytecode: core + all derived forms (via Expander) + `call/cc`, `dynamic-wind`, `guard`,
  `parameterize`; `BytecodeMatchesASTInterpreter` gives 5 differential tests vs tree-walk (88 total).
- Toy VM: desugars via Expander but **continuations are broken** (`call/cc` returns garbage —
  §2.1); demo-grade.
- CoreIR: incomplete (lambda exec TODO coreir.h:144; no local-var lowering; inline-clone throws),
  **0 tests, no REPL mode, unreached** — integral-to-complete per owner, not dead.

**Recommendations (priority)**
1. Break the ZombieBytecode↔ZombieCompilerIR cycle: move the bytecode `Compiler` into
   `ZombieBytecode` (VM owns its compiler), leave `ZombieCompilerIR` as the optional CoreIR
   pipeline depending one-way on ZombieBytecode. Removes the circular link.
2. Route the tree-walk through the shared `Expander` front-end (or extract a single
   derived-form desugarer both use) so semantics can't drift; delete the inline duplicates.
3. Add differential tests for toy VM and CoreIR against the tree-walk (CoreIR has none); fix
   §2.1 toy-VM continuations or formally mark it demo-only.
4. Cleanup: remove the empty `ZombieBytecode/ZombieBytecode/` directory.

## Fix log (2026-11-13)

- [x] **Evaluator layering #1 — broke the ZombieBytecode↔ZombieCompilerIR cycle.** Moved
      the bytecode `Compiler` (`compiler.{h,cpp}`) from `ZombieCompilerIR` into
      `ZombieBytecode` (where its VM lives); it references no CoreIR types, only
      ZombieRuntime + the local opcode header. Dropped the reverse CMake link
      (`ZombieBytecode PUBLIC ZombieCompilerIR`). Now one-way: `ZombieCompilerIR →
      ZombieBytecode` (CoreIR lowers to bytecode). Build order is topological; 88 bytecode
      tests + full 1257 pass.

- [x] **Value Ledger / "Multiverse Workbench" pruned entirely** — ~3,400 lines of a
      grand, mostly-stubbed vision (per-value provenance; Scheme/Native/Bytecode/Dream
      "realities"; swappable skins; cross-language mirrors; time-travel). Only the Scheme
      slice was built; the rest was permanently TODO. Removed: `ledger.{h,cpp}`,
      `mirror.{h,cpp}`, `scheme_skin.{h,cpp}`, `value.h`, `primitives/{primitive,primitives}_value_ledger.*`,
      `LispTest/ValueLedgerPrimitivesTests.cpp`, `ZombieLib/VALUE_LEDGER_README.md`.
      Un-wired from the core: dropped the per-`Object` `m_value_id` field (object.h/.cpp —
      every object is now 8 bytes smaller), the ledger/mirror init in `constants.cpp`, the
      dead `g_lisp` global (lisp.h/.cpp), the install in `primitive.cpp`, the `ledger()`/
      `mirrors()` accessors in `runtime_context.*`, and the `Value`/mirror machinery that
      had leaked into `export_registry.h` (unused by the Scheme-facing export primitives —
      embedding stays intact). **1257/1257 pass (was 1288; −31 ledger tests), 0 leaks/0
      errors under ASan.** CoreIR, toy VM, and the embedding/export system were kept.

- [x] **`set!` returned the environment's internal value-list** — `(set! x 99)` in the
      REPL printed `(99 (**BASE**) (**BASE**) …)`. `Environment::set()` returned
      `set_first(valList, val)`, i.e. the frame's *values* list, which leaked out as
      `set!`'s result. Fixed at the source: `Environment::set` now mutates the binding
      and returns the assigned `val`. Covers both tree-walk `set!` paths (`new_eval`
      lisp.cpp:456 and the `eval_step` trampoline lisp.cpp:1245); the letrec and toy-VM
      callers discard the return. REPL now shows `99`; 1288/1288 still pass.

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

- [~] **§3.2 CRITICAL GCPolicy allocates outside GC tracking** — ~~fixed by making
      `GCPolicy::allocate<T>` delegate to `get_current_gc()->allocate<T>()`~~
      **SUPERSEDED / REMOVED (2026-11-13):** the whole `MemoryManagement/` abstraction
      (a shared GC-interface layer for an abandoned external scene-graph engine) was
      deleted, taking `GCPolicy.h` and its `GCPolicyIntegration` tests with it. See the
      "MemoryManagement prune" entry below. No action remains here.

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

- [x] **MemoryManagement prune (2026-11-13)** — `MemoryManagement/` was a shared
      GC-interface layer (`memory::IMemoryManager`/`ITracer`/`IWriteBarrier`/
      `IRootManager` + a policy system) intended for an external scene-graph engine
      that has been abandoned. With no second consumer it was pure indirection, so it
      was **deleted in full**. `IGarbageCollector` (gc_interface.h) was decoupled first:
      the 4 `memory::` base classes were dropped and its allocate/deallocate/visit/
      visit_field/add_root/remove_root/name/bytes_allocated/write_barrier methods are
      now declared directly (`override` → explicit `virtual`). Also deleted as dead
      collateral: `LispTest/GCPolicyTests.cpp`, `GarbageCollector/gc_concepts.h`,
      `GarbageCollector/gc_traceable.h`. Builds clean at `-O0` and ASan; **1288/1288
      pass, 0 leaks, 0 errors** (down from 1291: the 3 `GCPolicyIntegration` tests went
      with GCPolicy; the 2 P1 regression tests remain).

- [x] **Dead-code prune, round 2 (2026-11-13)** — removed abandoned/uncalled files
      surfaced while finishing the MemoryManagement prune:
      · `ZombieRuntime/primitives/primitives_scenegraph.{cpp,h}` — scenegraph Scheme
        bindings for the abandoned external engine; body was `#ifdef ENABLE_SCENEGRAPH`
        (off) over absent `scenegraph::` headers, `install_scenegraph_group` had no
        caller. Removed, and dropped its now-vestigial `#include` from `primitive.cpp`.
      · `GarbageCollector/gc_ergonomics_demo_v2.cpp` — standalone demo (own `main()`),
        already glob-excluded; removed the file and the CMake exclude line.
      · `ZombieRuntime/value_demo.cpp` — compiled-but-never-called demo of the (live)
        value-ledger system; no external refs. Removed.
      Builds clean at `-O0` (all targets incl. `repl`); 1288/1288 pass (no tests removed).

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

## 0. Two P1 GC bugs (allocation can return freed objects) — ✅ FIXED (2026-11-13)

Both verified empirically: each regression test was confirmed to FAIL with the fix
reverted and PASS with it, and both are clean under ASan.

### 0.1 [P1] Allocation can return a freed object (collect-before-root)  ✅ FIXED
`gc_manager_template.h` on_allocate triggers a collection when the threshold is
exceeded, but the just-allocated object is tracked yet **not reachable from any
root** (the caller hasn't received it), so the collection swept it and `allocate()`
returned a dangling pointer. Reproduces with `gc_threshold == 0`.
NOTE: the real fix site was **on_allocate**, not the shared `allocate<T>` template —
the concrete managers (`gc_manager_t<>` primary and generational) each define their
own `allocate<T>` that shadows `IGarbageCollector::allocate<T>`, so a fix there was
dead code. Fix: in each on_allocate, root the `obj` parameter's slot across
`collect()` (`add_root(&obj); collect(); remove_root(&obj);`). Done in both
`gc_manager_template.h` (primary `gc_manager_t`) and `gc_manager.cpp` (production
`gc_manager`); the base `allocate<T>` just documents that on_allocate owns this.
Only paid when a collection actually fires. (Generational `allocate_young` is a
separate *moving* path not cited here and left alone — a local-slot root can't
update the caller's pointer across a move.)
Test: `GCAllocatorTest.AllocationTriggeringCollectionIsNotReclaimed`.

### 0.2 [P1] Promoted objects' children skipped during minor collection  ✅ FIXED
`generational_gc_strategy.cpp` `copy_object_cheney` promoted an object to old gen
and returned without advancing `free`, so the promoted object never entered the
to-space `[scan, free)` range the Cheney loop walks — its children stayed in
from-space and dangled after the space swap. Fix: record promotions in a new
`m_promoted_this_cycle`; `minor_collect()` now drains it (scanning each promoted
object's children) interleaved with the to-space scan until a joint fixpoint, and
`rebuild_remembered_set()` additionally considers promoted objects so any that still
point to a young survivor become old→young remembered-set entries.
Test: `GCAllocatorTest.PromotedObjectChildrenAreForwarded` (discriminator: the
promoted parent's child pointer must be forwarded to a new young-gen address).

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
  `gc_box` tracking → objects invisible to GC / double-free risk.  🗑️ MOOT — file
  deleted in the MemoryManagement prune (see fix log). No longer in the tree.
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
Current: **the `LispTest` binary is 1288/1288, zero memory errors, zero leaks under
`-fsanitize=address`** (2026-11-13, after the MemoryManagement prune: was 1291 — the 3
`GCPolicyIntegration` tests were deleted with GCPolicy; the 2 P1 regression tests remain).
The GC suite (`*GC*` filter) is fully clean under ASan at both `-O0` and ASan builds.

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

> **Full-binary leak status (2026-11-13): ✅ LEAK-CLEAN.** The complete `LispTest`
> binary under ASan is now **1291/1291 pass, zero memory errors, zero leaks** (down
> from 303 leaks / ~69 KB at baseline). Fixes applied (see Fix log §5-leaks below):
> - `atom.h` `atom<T>::clone()` and both `cast_atom<>` overloads: raw `new atom<T>`
>   → `gc_new<atom<T>>` (the bulk — `clone()`, `is_equal`, numeric-tower comparisons).
> - `values_object.h` `ValuesObject::clone()` and `primitive_control.cpp` `(values …)`:
>   raw `new ValuesObject` → `gc_new<ValuesObject>` (the `call-with-values` leaks in
>   `MissingR5RSTests`/`NumericTowerTests`).
> - `LispTest/BytecodeTests.cpp`: `TearDown()` now `delete vm;` (the biggest leak — the
>   non-GC `BytecodeVM` plus its wind stack / internal buffers leaked every test), and
>   `BytecodeMatchesASTInterpreter` uses `gc->allocate` + `gc_root_guard` instead of
>   raw `new Lisp()` / `new Environment()`.
>
> Why `gc_new` fixes the "leak": GC-tracked objects are held in the global `gc`'s
> object list, which LSan reaches from the global `gc` pointer — so they count as
> reachable, not leaked. The raw-`new` clones bypassed the GC entirely and were never
> freed. Converting them is also the correct ownership model (GC owns clone lifetime).
> Routine ASan runs no longer need `detect_leaks=0`.

Older suppressions, now partly resolved:
- `and`/`or` tests commented out (§1.1).  ✅ re-enabled
- `GCBoxTests.cpp` excluded from build (stale `add_root(object*)` API).
- 9 `GCAllocatorTest.*` excluded from ctest (§3.1).  ✅ fixed and re-enabled
- 4 `exit`/`emergency-exit` tests commented out (would terminate the process —
  legitimate; could be re-enabled via `EXPECT_EXIT`/death tests).
- The file-IO suites share fixed scratch filenames and are only safe under ctest
  because of the `RESOURCE_LOCK` added during the port (they'd race otherwise).

---

## 5b. Orphaned-header prune (deep scan 2026-11-13, DONE)

Dead-code sweep after the MemoryManagement prune found headers `#include`d by nothing.
**Removed (5):**
- `GarbageCollector/gc_new.h` — dead duplicate: `gc_new(args…)` is also defined in
  `gc_ptr.h` (the definition used everywhere); file was included nowhere.
- `GarbageCollector/gc_local.h` — scoped-root helper; sole consumer was the deleted
  `POLICY_DEMONSTRATION.cpp`.
- `GarbageCollector/gc_threshold_guard.h` — scoped-threshold helper; zero refs.
- `GarbageCollector/gc_containers.h` — GC-aware containers; zero real includes (the
  `gc_vector` mention in `auto_traceable.h` was a doc comment).
- `ZombieRuntime/math_primitives.h` — overlapped the live `numeric_runtime`.

**Kept — FALSE POSITIVE:** `ZombieRuntime/macro.h` is live — it is `#include`d as
**`Macro.h`** (capital M) by closure/expander/lisp/macro/primitive. The sweep missed it
because the grep was case-sensitive and `/mnt/c` is a case-insensitive FS (`macro.h` ==
`Macro.h`). **Lesson: on this tree, verify includes with `grep -i` before deleting.**
Deleted-then-restored from git; build confirms it's required.

Result: all targets build, **1288/1288 pass**.

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
