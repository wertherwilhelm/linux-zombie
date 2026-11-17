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

## Reader / IO / number primitive bugs (2026-11-15, found by running a real program)

Running a non-trivial user program (`experiments/algol-msweet.scm`, an
ALGOL/M-expression reader that drives the tokenizer char-by-char over string
ports) surfaced five real interpreter bugs — all now FIXED, with regression tests
in `LispTest/ReaderPrimitiveFixTests.cpp`. The program now runs end-to-end
(`square[x] = {x * x}` parses to `(define (square x) (* x x))` and evaluates;
`(square 9)` => 81).

1. **`read-char` consumed the whole line**, not one character — it called
   `get_line()` to "consume the character" (`primitive_io.cpp`). So the first
   `read-char` returned a char and every subsequent one returned EOF. Now uses
   `InputPort::get()` (one char). This single bug breaks all char-by-char input.
2. **`string->number` returned `nil`, not `#f`**, for non-numbers
   (`primitive_numbers.cpp` stringToNumber) — and `nil` is TRUTHY here, so the
   ubiquitous idiom `(or (string->number s) default)` always took the number
   branch and yielded `()`. Now returns `false_obj`.
3. **`#\;` / `#\(` / `#\)` char literals mis-read** — `read_char` stopped the
   char name at any delimiter, so these read an empty name ("empty character
   literal", or `;` treated as a comment). Now always takes the first char
   literally; only a letter starts a multi-char name (`space`, `newline`, ...).
4. **`eof-object?` was registered as `eof-Object?`** (mis-cased) and `eof-object`
   was missing. Renamed to the R7RS names, added `eof-object`, exported both from
   `(scheme base)`; updated ~18 test usages of the old name.
5. **`load` ignored `;` comments** — it used a hand-rolled paren-scanner that read
   comment text as code ("Unbound variable: <word>"), and also mishandled `#\(`
   char literals. Now parses via `scheme_reader::read_program_from_string` (the
   same comment-aware reader the REPL uses) and evaluates each form.

Known remaining (REPL smart-edit, not the reader): typing `#\;` or `#\(`
interactively still triggers the line-continuation prompt because the REPL's
smart-edit plugin does its own (comment/paren-naive) completeness check; `load`
and programmatic reading are unaffected. Minor: `string->number` parses integers
as inexact (`"7"` -> 7.0 via stod); R7RS-exactness for integer strings is a
separate improvement.

---

## Corpus test runner (2026-11-16) + gaps it discovered

Added a data-driven corpus runner (`LispTest/CorpusTests.cpp`, dir
`corpus/<name>/{source.scm,cases.scm}`, path via `ZOMBIE_CORPUS_DIR`). Each
`cases.scm` defines `corpus-cases` = list of `(label form expected [status])`;
the runner loads source+cases into a fresh `scheme_engine`, evaluates each form,
and compares printed result to `expected`. A 4th element `'gap` marks a known
missing area: reported (`[gap] ...`) but not a failure, and flagged if it starts
passing (so the tag gets dropped). Adding a test = editing data, no recompile.
Entries: `algol-msweet` (the reader, 8 cases, all pass) and `r7rs-core` (42
feature probes).

Three real bugs FIXED while building it (vector support), with regression tests
in `ReaderPrimitiveFixTests.cpp`:
- `#(...)` **vector literals** were rejected by the reader (yet vectors exist as
  values and print as `#(...)`). Added to `read_char`.
- **vector/bytevector constants now self-evaluate** (unquoted `#(1 2 3)` threw
  "Unexpected atomic type" in the tree-walk `eval_atom`). NOTE: only fixed in the
  tree-walk; bytecode/CoreIR/toy-VM self-eval of `#(...)` not yet checked.
- **`equal?` now compares vectors element-wise** (was pointer-only -> `#f`).

Gaps the `r7rs-core` corpus flagged — ALL NOW CLOSED (2026-11-16), corpus green
with zero `'gap` tags; regression tests in `LispTest/StdlibGapTests.cpp`:
- `reduce`, `sort`, `list-sort` — added to `PrimitiveControl` (higher-order; sort
  is a stable_sort driven by the comparison procedure).
- `string-split`, `string-contains` — added to `PrimitiveStrings`.
- `floor/` / `truncate/` / `exact-integer-sqrt` now return **two values**
  (`ValuesObject`) per R7RS; the ~30 numeric tests that treated them as single
  values were updated to `call-with-values`.
- **Hash tables** (SRFI-69 subset) — new `HashTable` Object type
  (`hash_table.h`, `atype::HASHTABLE`, alist-backed with `equal?` keys, GC-traced)
  + `PrimitiveHashTables` group: make-hash-table, hash-table? , -set!, -ref
  (value or thunk default), -ref/default, -delete!, -contains?, -size, -keys,
  -values, ->alist.

### Corpus batch 2 (`corpus/r7rs-extra`, 2026-11-16)

A broader probe set (49 cases). Confirmed working (now regression-covered): chars
(whitespace/alphabetic/numeric/ci=?), vector-copy/-append, square/exact/
numerator/denominator, delay/force, guard, with-exception-handler, parameterize,
dynamic-wind, assv, list-copy. Closed the R7RS-small **core** gaps it found:
- `exact-integer?`, `nan?`, `infinite?`, `finite?` (PrimitiveNumbers).
- `digit-value` (PrimitiveCharacters).
- `error-object?` / `error-object-message` / `error-object-irritants` — existed
  but were mis-cased `error-Object*` (same bug class as `eof-Object?`); renamed.
- **Inexact division by zero no longer errors** — `(/ 1.0 0.0)` -> `+inf.0`,
  `(/ 0.0 0.0)` -> `+nan.0` (R7RS); exact `(/ 1 0)` still errors. (numeric::div.)

**Prelude wired (2026-11-16):** `scheme_engine::warmup_prelude()` (previously a
no-op stub) now evaluates a Scheme SRFI-1/SRFI-13 prelude into `global_env_` after
primitives+evaluator are ready; called from every constructor and `reset()`. So
the REPL, embedding, and the corpus get: iota, take, drop, last, count, delete,
remove, partition, delete-duplicates, find, any, every, append-map, list-index,
list-tabulate (SRFI-1); string-reverse, string-join, string-index, string-prefix?,
string-suffix?, string-trim, string-pad (SRFI-13); vector-sort (R6RS). All built
on existing primitives; works on all four evaluators. The r7rs-extra corpus
entries are now untagged (pass); only `rationalize` remains `'gap` (returns
inexact — a numeric-runtime fix, not a prelude one).
- Note: the prelude loads per `scheme_engine` construction; the differential
  suite (fresh engine per case × evaluator) pushed the full-suite time ~43s->62s.
  If the corpus/differential grow, cache the parsed prelude or add a no-prelude
  engine mode for the differential (it uses only primitives).

### Corpus batch 3 (`corpus/r7rs-syntax` 21 cases, `corpus/r7rs-io` 19 cases, 2026-11-16)
Probed macros/derived-syntax/control forms and ports/IO. Confirmed working:
syntax-rules (incl. ellipsis, let-syntax, letrec-syntax), case-lambda, do/case/
cond=>/when/unless/and/or/named-let/let*/letrec, quasiquote unquote+splice,
string input ports (read/read-char/peek-char/read-line/read-string), port
predicates. Gaps it discovered and their disposition:
- **`write` used display semantics even to a port** (real bug, FIXED). `write`
  now serializes in machine-readable form — strings quoted+escaped, chars as
  `#\name`/`#\c`, applied recursively through pairs and vectors — via a
  file-local `write_datum` in `primitive_io.cpp`; `display` is unchanged. Fixes
  out-string-write, write-string-quoted, write-char-literal, read-write-roundtrip.
- **`read` used a second, inferior line-based tokenizer/parser** (`InputPort::
  tokenize`/`parse`) that couldn't handle `#(` vectors, `#\` chars, quote
  shorthand, dotted pairs, or multi-line/multi-datum input (real bug, FIXED).
  `InputPort::read()` now routes through the real `scheme_reader`: for a seekable
  stream it slurps the remaining content, parses one datum via a new
  `read_from_string(src, consumed)` overload, and `seekg`s back to just past it
  so sequential reads advance correctly. Legacy tokenizer kept only as the
  non-seekable (interactive stdin) fallback. Regression tests in
  `ReaderPrimitiveFixTests.cpp` (WriteVsDisplaySemantics, ReadPrimitiveFullGrammar).
- Still `'gap` (tagged, non-trivial): `with-output-to-string` (needs
  current-output-port parameterization).

### Quasiquote rewrite + tree-walk unification (2026-11-16)
`Expander::expand_quasiquote` is now depth-threaded: it descends into vector
literals (`` `#(1 ,(+ 1 1) 3) `` → `#(1 2 3)`, rebuilt via `list->vector`) and
tracks nesting so an unquote is only evaluated at the matching level
(`` `(1 `(2 ,(+ 1 2))) `` preserves the inner `(unquote (+ 1 2))`). The tree-walk
evaluator's parallel `qq_expand`/`qq_expand_list` (which did neither, so it
diverged from the other three evaluators) was removed; `quasiquote` now routes
through the shared Expander like the other derived forms. `qq-vector`/`qq-nested`
corpus entries un-tagged; three entries added to the cross-evaluator differential.

### GC: scheme_engine leaked a stack-address persistent root (2026-11-16, CRITICAL)
Found via ASan while running the suites together (abort in
`MarkSweepStrategy::mark()` at mark_sweep_strategy.cpp:131). The constructor
registered `&global_env_` — the address of a member of a (usually stack-allocated)
`scheme_engine` — as a persistent GC root, but `~scheme_engine()` never
unregistered it ("global_env_ will be cleaned up by GC"). After a stack engine was
destroyed, the slot address was reused, so the next collection (in a later test)
dereferenced freed stack memory. Fix: capture the registering GC in
`env_root_gc_` and `unregister_persistent_root(&global_env_)` in the destructor.
Full suite now ASan-clean (1321/1321). Note the "robust safety check" in `mark()`
validates the root *value* but still dereferences the *slot* first, so it could
not have caught this — the slot lifetime is the real invariant.

### Corpus batch 4 (`corpus/r7rs-records`, 2026-11-16)
Probed records, exceptions, parameters, and higher-order ops. Working:
`parameterize`/`make-parameter`, `dynamic-wind`, `guard`/`raise`/
`raise-continuable`/`error`/`error-object?`/`-message`/`-irritants`,
`vector-map`/`vector-for-each`/`string-map`, and `map`/`for-each` over multiple
lists. Gaps found and closed:
- **`define-record-type` was entirely missing** (unbound on every evaluator).
  Implemented as a derived form in the shared Expander
  (`expand_define_record_type`): desugars to a `begin` of defines over a
  tagged-vector representation — `(define <tag> (list '<name>))` for a unique
  per-type identity, a `(vector <tag> slot...)` constructor (unset slots → #f),
  a `(and (vector? o) (= (vector-length o) N+1) (eq? (vector-ref o 0) <tag>))`
  predicate, and `vector-ref`/`vector-set!` accessors/modifiers. Uses only core
  primitives, so all four evaluators get it; tree-walk delegates it via
  `kExpanderForms`. Corpus `corpus/r7rs-records` (6 cases) + differential entries.
- **CoreIR couldn't lower self-evaluating vector literals** (`#(...)` → "Expected
  list expression"); surfaced while testing records. `is_atomic()` in
  `scheme_to_coreir.cpp` omitted VECTOR/BYTEVECTOR, so a bare vector literal was
  neither atomic nor a pair and fell through. Added them. (`'#(...)` and
  `(vector ...)` already worked.) Differential entries added.
### Corpus batch 5 (`corpus/r7rs-bytevectors`, 2026-11-16)
Closed the `#u8(...)` reader gap and two related bugs it exposed:
- **Reader**: `scheme_reader::read_char()` (the `#`-dispatch) now handles
  `#u8(byte ...)` → `a_bytevector`, validating each element is an exact integer
  in [0,255]. Mirrors the `#(` vector-literal path.
- **`equal?`** gained a bytevector branch in `base.cpp` (byte-wise compare) —
  it was pointer-only like vectors used to be, so `(equal? #u8(1 2 3)
  (bytevector 1 2 3))` was #f.
- **Noisy diagnostic**: the shared argument extraction in
  `primitive_bytevectors.cpp` computed `y`/`z` via `rest(rest(args))` even for
  1-arg calls, so `rest(nil)` printed "cannot take rest of an atom!" to stderr
  on every `bytevector?` / `bytevector-length` call (benign — right answer still
  returned, which is why tests never caught it). Rewrote it to walk pair-by-pair.
Works on all four evaluators (CoreIR already lowered bytevector literals after
the batch-4 `is_atomic` fix). Corpus `corpus/r7rs-bytevectors` (11 cases) +
`ReaderPrimitiveFixTest.BytevectorLiterals` + differential entries. 1322/1322.

### Corpus batch 6 (`corpus/r7rs-numeric-strings`, 2026-11-16)
Probed numeric rounding, radix parsing, and optional range/comparator args.
Closed:
- **`round` used round-half-away-from-zero** (`std::round`) instead of R7RS
  ties-to-even, and always returned inexact. Now uses `std::nearbyint`
  (FE_TONEAREST = round-even) and preserves exactness: integral/exact inputs
  give exact results (`(round 2.5)`→2, `(round 7/2)`→exact 4).
- **`string->number` rejected every explicit radix**: the radix check was
  `is_double(y)`, but a radix (2/8/10/16) is an exact integer. Switched to
  `is_number` + `to_int64`; also made the radix-10 path parse exact integers
  first (so `"42"`→exact 42) before falling back to a float parse.
- **`string-copy` / `string->list`** were arity (1,1) — added the optional
  `[start [end]]` substring arguments (now (1,3)).
- **`assoc` / `member`** were arity (2,2) — added the optional user comparator
  (now (2,3)), invoked as `(compare obj element)` via a new `member_assoc_cmp`.
All work on every evaluator. Corpus `corpus/r7rs-numeric-strings` (26 cases) +
differential entries. Full suite 1322/1322, ASan-clean.
- Minor still-open: `(min 1 2.0)` should be inexact (1.0) by contagion but
  returns exact 1; `#x`/`#b` number prefixes in the reader aren't parsed.

### Corpus batch 7 (`corpus/r7rs-seq-ops`, 2026-11-16)
- **floor/ceiling/truncate** had the same exactness bug as round (always
  returned inexact). Now exact/integral inputs give exact results
  (`(floor 7/2)`→exact 3), inexact stays inexact.
- **`symbol=?`** was missing — added to `primitive_symbols` (variadic, 2+ args;
  the group's constructor now picks arity per id).
- **`define-values`** was missing — added as a derived form in the Expander
  (`expand_define_values`): defines each variable then `call-with-values` on a
  producer thunk `set!`s them. Handles fixed, dotted-rest, and single-symbol
  formals. Tree-walk delegates via `kExpanderForms`.
- **`vector->list`**/**`vector-fill!`** gained their optional `[start [end]]`
  args; **`vector-copy!`** (`to at from [start [end]]`) was missing entirely —
  added with overlap-safe copy direction.
All work on every evaluator. Corpus `corpus/r7rs-seq-ops` (19 cases) +
differential entries. Full suite 1322/1322, ASan-clean.

### Corpus batch 8 (`corpus/r7rs-comparisons`, 2026-11-16)
- **char / string comparison predicates were binary-only** (arity 2,2); R7RS
  makes `char=?`/`<?`/`>?`/`<=?`/`>=?` (and ci variants) and the string variants
  variadic chains. Added `charCompareChain`/`stringCompareChain` helpers (hold
  iff every adjacent pair holds) and widened the arity to (2, n).
- **`boolean=?`** was binary-only — made variadic.
- **min/max inexactness contagion**: `(min 1 2.0)` returned exact 1; R7RS says
  the result is inexact if any argument is inexact. `num_compute` now tracks it
  and coerces the extremum to inexact. (Note: an inexact integer-valued double
  still *prints* without a `.0` here — a separate printer cosmetic gap.)
All work on every evaluator. Corpus `corpus/r7rs-comparisons` (16 cases) +
differential entries. Full suite 1322/1322, ASan-clean.

All work on every evaluator. Corpus `corpus/r7rs-comparisons` (16 cases) +
differential entries. Full suite 1322/1322, ASan-clean.

### Corpus batch 9 (`corpus/r7rs-inexact-print`, 2026-11-16)
- **Inexact reals printed without a decimal point** (`1.0`→"1"), so a written
  value read back as an exact integer — a real external-representation bug.
  Added an `atom<double>::print` specialization (atom.h) that appends `.0` when
  the default formatting has no `.`/`e`, and emits `+inf.0`/`-inf.0`/`+nan.0`.
  `number->string` (radix 10) now delegates to the value's own `print` so it
  shares this (it had a separate formatter that printed inexact integers bare).
- **`expt` was always inexact** — `(expt 2 10)`→1024.0. R7RS: exact base with an
  exact integer exponent gives an exact result (integer for e≥0, rational for
  e<0). Added that path (repeated `numeric::mul`, `div` for negatives); other
  cases still use `std::pow`. This was masked until the printing fix exposed it.
- Fixing the printer also corrected several of my own earlier corpus/differential
  expectations that assumed the buggy bare-integer printing for inexact `round`
  results (`(round 2.5)` is inexact → "2.0").
All on every evaluator. Corpus `corpus/r7rs-inexact-print` (17 cases) +
differential entries. Full suite 1322/1322, ASan-clean.

### Corpus batch 10 (`corpus/r7rs-case-hashupdate`, 2026-11-16)
- **`case` with `=>`** errored — `expand_case` now recognises a
  `(datums => proc)` / `(else => proc)` clause and rewrites the body to
  `(proc key)` (R7RS 4.2.1).
- **`hash-table-update!`** and **`hash-table-update!/default`** were missing —
  added to `primitive_hashtables` (`new = (proc current)`, where current is the
  existing value or the failure-thunk's result / default when absent; SRFI-69).
All on every evaluator. Corpus `corpus/r7rs-case-hashupdate` (8 cases) +
differential entries. Full suite 1322/1322, ASan-clean.
- Note: at the **CoreIR REPL top level** a bare `if`/`cond`/`case` errors with
  "if in operand position not supported" — a pre-existing REPL-path limitation
  (plain `(list (if #t 1 2))` fails too); the differential harness and
  in-`define` uses lower it fine, so this is orthogonal to `case =>`.

### Known-gap closures (2026-11-16)
- **Number prefixes `#x`/`#b`/`#o`/`#d`/`#e`/`#i`** — the reader rejected all of
  them. `scheme_reader::read_char` now parses radix + exactness prefixes
  (combinable, either order) via `parse_number_token`. New corpus
  `corpus/r7rs-number-prefixes` (17) + differential. On every evaluator.
- **`rationalize`** returned inexact for exact inputs. Reimplemented in the
  prelude (the "simplest rational" algorithm over the exact tower — now correct
  because floor/`/`/`-` are exactness-preserving), shadowing the inexact C++
  primitive. Exact on tree-walk/bytecode/vm; **CoreIR still falls back to the
  inexact primitive** because the SRFI prelude doesn't load there (see below).
  Untagged in `corpus/r7rs-extra`.
- **`with-output-to-string` / `call-with-output-string`** were missing — added
  as IO primitives that bind the default output port (`stdout_port`) to a fresh
  `StringOutputPort` for the dynamic extent of the thunk/proc and return the
  captured text. On every evaluator. Untagged in `corpus/r7rs-io`.

**CoreIR "if in operand position" is the key remaining architectural gap.**
Root cause confirmed: the CoreIR lowering can't handle `if` (hence cond/case,
and anything that desugars to them) when it appears as a sub-expression — an
argument, or a `let`-binding init. `(list (if #t 1 2))` fails; the prelude's
very first form (`iota`, which has `(if ...)` as a `let` init) fails, so the
whole SRFI prelude aborts loading on CoreIR (iota/take/string-*/rationalize all
unbound there). Fixing it needs an ANF-style lift of `if` into a join-point
`let` during lowering. Everything works in tail/statement position and via the
non-REPL differential harness, so it's isolated but worth doing next.

### Corpus batch 11 (`corpus/r7rs-srfi-extras`, 2026-11-16) — shared-layer fills
All targeting the generalized layers (reader/expander/prelude), per the
architecture intent (see memory linux-zombie-architecture):
- **Named character literals** `#\null`/`#\alarm`/`#\backspace`/`#\delete`/
  `#\escape` (+ aliases nul/rubout/esc) and `#\xHH` hex scalar — the reader only
  knew space/newline/tab/return. Added to `scheme_reader::read_char`. All 4.
- **`receive`** (SRFI-8) — added as an Expander derived form
  `(call-with-values (lambda () expr) (lambda formals body))`. All 4.
- **SRFI-1 prelude**: `fold`, `filter-map`, `concatenate`, `take-while`,
  `drop-while`, `span`, `break`, `unfold`. **SRFI-69 prelude**: `hash-table-walk`,
  `hash-table-fold`, `hash-table->list`, `hash-table-copy`, `alist->hash-table`
  (built on the hash-table primitives). tree-walk/bytecode/vm (prelude not on
  CoreIR yet — the if-in-operand lowering gap).
Corpus `corpus/r7rs-srfi-extras` (19 cases) + differential (reader/receive).
Full suite 1322/1322, ASan-clean.

Corpus now has 15 entries. **No `'gap` tags remain anywhere.** Reader + Expander
are the dense shared layers (every derived form / lexical feature added there is
all-4 at once); the Scheme prelude is all-4 *except CoreIR* until the CoreIR
lowering handles `if` in operand position (the hinge for the next phase).

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
- Toy VM: desugars via Expander; **first-class `call/cc` + `dynamic-wind` work** (119 tests;
  see §2.1). Still labelled experimental, but continuations are functional, not broken.
- CoreIR: incomplete (lambda exec TODO coreir.h:144; no local-var lowering; inline-clone throws),
  **0 tests, no REPL mode, unreached** — integral-to-complete per owner, not dead.

**Recommendations (priority)**
1. ✅ DONE — broke the ZombieBytecode↔ZombieCompilerIR cycle (Compiler moved into ZombieBytecode).
2. Route the tree-walk through the shared `Expander` front-end (or extract a single
   derived-form desugarer both use) so semantics can't drift; delete the inline duplicates.
   Prereq done: eval/new_eval consolidated to a single public `eval` boundary. Remaining
   wrinkle: `eval` is re-entered for closure bodies/macros, and the Expander does eager
   compile-time macro handling vs the tree-walk's lazy model (see the earlier #2 analysis).
   (✅ The `eval_list` vs `eval_step_list` internal duplication is now resolved —
   `eval_step_list`'s trampoline is the single special-form dispatcher; `eval_list` was
   stripped to just the `guard`/`parameterize` handlers it uniquely serves, −599 lines.)
3. Add differential tests for CoreIR against the tree-walk (it has none). (Toy-VM
   continuations already work — §2.1 — and have 119 tests.)
4. ✅ DONE — empty directories removed.

## Fix log (2026-11-13)

- [x] **Cleanup batch** — (1) removed orphaned `auto_traceable.h`; (2) removed 7 empty
      port-cruft directories (no commit — git doesn't track empty dirs); (3) re-enabled
      `GCDeepListNestingTCO` (was `#if 0` for the "conservative GC limitation" now fixed);
      (4) **eval/new_eval consolidation** — `Lisp::eval` (IEvaluator) is now the one public
      evaluation entry; migrated the external/test callers off `new_eval`, renamed it to
      `eval_impl`, and made the whole tree-walk machinery private. Behavior-preserving;
      1248/1248 pass. This gives #2 (front-end unification) a single boundary to build on,
      though `eval` is still re-entered for closure bodies/macros — the remaining wrinkle.


- [x] **Removed the necromancy / epitaph / graveyard "death" feature.** Whimsical
      per-object death-notification system; pruned on request. Deleted
      `ZombieRuntime/last_words.{h,cpp}`, `primitives/primitive_necromancy.{h,cpp}`,
      `LispTest/NecromancyTests.cpp`. Un-wired: the GC obituary-hook mechanism itself
      (`IGarbageCollector::set_obituary_hook` + `m_obituary_hook` + the hook call in
      `gc_manager::reclaim`, which now just deallocates), `install_obituary_hook` in
      `constants.cpp`, and the `PrimitiveNecromancy::install` call in `primitive.cpp`.
      Only necromancy used the hook. `-O0` 1247/1247 (−10 necromancy tests; the one
      DISABLED epitaph test went with it). (This also retires the conservative-GC
      awkwardness around that disabled test.)


- [!] **Evaluator layering #2 — unify front-end via shared Expander: ATTEMPTED, REVERTED.**
      Routing the tree-walk through `Expander` (expander.cpp) is **not cleanly viable** in the
      current architecture: (1) there is no single evaluation boundary — the test fixture
      (`TestFixture.h:100`) and internal code call `new_eval` **directly**, bypassing `eval()`,
      so a boundary-level `expand()` can't be validated by the suite and stage-2's deletion of
      the inline handlers would break the test path (derived forms would reach `new_eval` with
      no handler and no expansion); (2) the `Expander` does **eager, expand-time** macro
      registration/expansion (`define-syntax` → `env->define` + returns `(begin)`; expands user
      macro calls), a different model than the tree-walk's lazy runtime macros; (3) it only
      partially overlaps (keeps named-`let`, skips parameterize/delay/define-library/import).
      Proper fix is a deliberate refactor: either establish ONE evaluation entry, or extract a
      pure *structural* desugarer (no macro handling) that both the Expander and `new_eval`'s
      dispatch call per-form. Deferred. (#1 — the dependency-cycle break — stands and is good.)

- [x] **FIXED — deep-loop corruption (precise GC vs. conservative-assuming code).**
      The runtime GC marks only explicit roots but the reader/evaluator/desugaring keep live
      `Object*` in unrooted C++ locals, so a collection mid-operation reclaimed them
      (use-after-free; "Unbound variable: +"; "cannot take first/rest of an atom"). Fix, in
      layers (ASan + low-threshold repro `GCEdgeCaseTests::DeepLoopGCDoesNotCorruptLiveState`):
      1. `scheme_reader::read_list()` + `read_program_from_string()` — rooted the in-progress
         list head and each element across `read_expr()`/`cons()`.
      2. `MarkSweepStrategy` — added **conservative stack scanning** (`mark_conservative_stack`):
         spills registers via setjmp, scans [current SP .. `__libc_stack_end`) word-by-word, and
         marks any word that is exactly a known-live object. `no_sanitize_address` on that fn so
         ASan's stack red zones don't false-trip. Safe for this non-moving collector (objects
         never relocate; false positives just retain one extra cycle).
      3. **Opt-in per instance** (`set_scan_stack`, default OFF): only the process-wide runtime
         GC (`initialize_gc`) enables it; isolated `gc_manager`/`gc_manager_t` instances
         (tests/embedding with explicit roots) stay PRECISE, so their exact-collection assertions
         still hold.
      4. Reconciled the few collection-assertion tests: `GCRootsDoNotLeakAfterRemoval` now uses an
         isolated precise `gc_manager_t`; `NecromancyTest.EpitaphIsSpokenAtDeath` DISABLED (asserting
         a *specific* object's prompt death is not guaranteeable under a conservative collector —
         feature still works; a C++-level precise-gc obituary test would be the deterministic
         replacement).
      Result: `-O0` and ASan both **1257/1257, 0 leaks / 0 errors**; the deep-loop repro that
      used to corrupt now passes under ASan. The REPL deep-loop corruption is gone.

- [x] ~~**ROOT CAUSE FOUND — precise GC vs. conservative-assuming code (deep-loop corruption).**~~ (fixed above)
      Chased the deep-loop corruption with ASan + a low-`gc_threshold` repro
      (`GCEdgeCaseTests.cpp: DeepLoopGCStress_TEMP`). The crashes are a chain of
      **unrooted C++ locals holding live `Object*` across allocations that trigger GC**:
      · `scheme_reader::read_list()` and `read_program_from_string()` built the in-progress
        list in unrooted locals → GC during a later `read_expr()` swept it → next `set_cdr`
        hit freed memory. **FIXED** (rooted the head + each element; 1257 still pass).
      · Then `Lisp::eval_step_list` named-`let` desugaring (lisp.cpp:1471-1475) builds
        `vars`/`vals` via `cons` + `reverse`, unrooted → freed mid-build. **(not yet fixed)**
      Root cause: `MarkSweepStrategy::mark()` scans ONLY explicit roots (`m_gc_roots`,
      `m_persistent_roots`) — it does **not** scan the C++ stack, yet the header advertises
      "conservative root scanning" (mark_sweep_strategy.h:17) and the reader/evaluator/
      desugaring are written as if stack locals are automatically protected. So every
      unrooted intermediate is a latent use-after-free, triggered whenever GC fires mid-op
      (deep loops, or a low threshold). Masked in tests by the 100 MB threshold.
      **Strategic choice (see report):** (A) implement conservative stack scanning in the
      non-moving mark-sweep GC (validate stack words against `m_all_objects`; matches the
      documented intent; fixes the whole class at once) — recommended; or (B) stay precise
      and hand-root every intermediate across reader+evaluator+desugaring (endless whack-a-mole).

- [x] **HIGH (pre-existing): deep loops corrupt live state via the engine/REPL — FIXED.**
      `(let loop ((i 0)(acc 0)) (if (< i 100000) (loop (+ i 1)(+ acc i)) acc))` corrupted at
      ~10^5 iterations ("Unbound variable: +", "cannot take first/rest of an atom") in both
      `--lisp` and `--bytecode`. Root cause + fix: see the consolidated entry above
      (reader rooting + conservative stack scanning in the runtime GC). Verified gone.


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

### 2.1 Toy VM (`--vm`) — continuations WORK ✅ (audit was stale)
Re-verified 2026-11-14: first-class `call/cc` is fully functional in the toy VM via
`ContinuationSupport` (capture = copy the frame stack; invoke = restore it + rewind the
dynamic-wind stack). Confirmed: escape, non-escape, nested call/cc, re-invokable stored
continuations, `dynamic-wind`, and `dynamic-wind` + escape all behave correctly; **119
continuation/dynamic-wind/VM tests pass** (ContinuationTests run against the toy VM). The
old "returns garbage / not enabled" symptom no longer reproduces — it was fixed since the
original audit. The `enable_continuations=false` path (toy_vm.cpp throws "not enabled") is
only hit if a VM is constructed with continuations disabled; the REPL/engine enable them.

### 2.2 CoreIR (`ZombieCompilerIR`) — far more complete than the audit claimed
Re-verified 2026-11-14 by building the first end-to-end harness
(`LispTest/CoreIRTests.cpp`, 24 tests: Scheme → CoreIR → bytecode → BytecodeVM). The
pipeline was previously **unreached and untested**; the old "lambda execution TODO"
(coreir.h:144) and "no local lowering" (789) claims are **stale** — lambdas, closures,
captures, recursion (incl. multi-arg + mutual), `set!`/local mutation, named `let`,
`let*`, `letrec`, `begin`, `cond`, `and`/`or`, and list ops all work and match the
tree-walk differentially.
Bugs found + FIXED in this pass:
- Named `let` threw "Let binding variable must be symbol" → desugar to letrec+lambda.
- `quote` was mishandled in every atomic-lowering path (returned a literal of the whole
  `(quote X)` form → "Unbound variable:") → centralized into one `atomic_to_atom` helper.
- `if`/`cond` with **no else** dereferenced nil → now yield unspecified.
- `cond` with an **atomic test** (`#t`, a symbol) produced an empty result var → use
  `atomic_to_atom`.
- Pre-existing leak: `SchemeToCoreirLowering` never freed its `Expander` → `unique_ptr`.
- `'` reader shorthand was structurally different from `(quote X)`: the reader built the
  quote/quasiquote/unquote keywords with `gc_new<a_string>` instead of `intern_symbol`,
  so the keyword was a STRING not a SYMBOL (the tree-walk was lenient, CoreIR was not).
  Fixed in `scheme_reader.cpp` → now interned symbols (consistent with how `quote` reads
  elsewhere). This also fixed `'()`.
- quote was not stripped for **primitive arguments** (five copies of inline atomic-arg
  lowering, each doing `makeLit(arg)` on `(quote X)`): `(null? '())` saw a pair → `#f`,
  `(car '(1 2))` returned the symbol `quote`. All five now route through `atomic_to_atom`.
Now **30 CoreIR tests pass** (incl. differential vs tree-walk over quote/cond/null?/list
ops). Remaining unstarted: `inline_stmt_chain` clone (1449) and a JIT backend.

**CoreIR is now wired as a user-facing evaluator.** `coreir::CoreIrEvaluator`
(ZombieCompilerIR/coreir_evaluator.{h,cpp}) implements `IEvaluator` by running
lowering → `CoreIRToBytecode` → `BytecodeVM::execute` against the shared env (so
top-level `define`s persist across REPL lines). Selectable via
`EvaluatorType::COREIR` and the REPL `--coreir`/`-c` flag; covered by
`CoreIREngineTest` (engine-level tests).

**define return value unified across evaluators (R7RS: unspecified).**
Previously `define` evaluated to three different things: the tree-walk returned
the bound symbol, the bytecode VM returned the stored value (and for function
defines, the closure — which prints as its body, e.g. `(begin * y y)`), and
CoreIR returned `begin`. Per R7RS the value of a `define` is unspecified, so all
three now return the codebase's unspecified value `nil` (prints as `()`, same as
one-armed `if` / `when` / `unless` / `import`); `set!` keeps value semantics (a
deliberate, documented choice — `Environment::set` returns the assigned value).
- tree-walk: `lisp.cpp` performs `env->define(...)` for the side effect and
  returns `nil`.
- bytecode: `compile_define` appends `POP; LOAD_CONST nil` after the shared
  `STORE_VAR` (set! keeps STORE_VAR's pushed value).
- CoreIR: both define branches return `makeLit(nil, TypeHint::VOID)` after
  forcing the `SetGlobal` side effect via a temp binding.
`BytecodeVMTest.DynamicWindWithCallCCReentry` asserts on the recomputed binding
(`result1` == 120) and the wind-thunk trace rather than the continuation's
return value (a continuation captured inside `(define result1 ...)` now yields
the unspecified value on re-entry). Locked in by `*.DefineReturnsUnspecified`
(bytecode + CoreIR). Full suite 1284/1284; ASan clean on the affected suites.

---

## 2.3 Differential conformance across evaluators (2026-11-14)

The bulk of the ~1290 tests exercise only the tree-walk evaluator (via
`TestFixture.h`); bytecode / CoreIR / toy-VM were thinly covered, which is why
evaluators silently disagreed (set! return, null?/'(), define return were all
found by hand). Added `LispTest/DifferentialTests.cpp`: one corpus run through
all four evaluators, asserting each matches the tree-walk reference. Each case
carries an evaluator **mask**; evaluators with a known gap are masked out (not
executed — important because some gaps are hard crashes). The mask is the living
gap list: fix a gap, flip the bit to ALL, and that evaluator is guarded too.

A one-off subprocess sweep (repl binary, timeout-isolated) over ~85 programs
produced the punch-list (11 divergences). Status:

- **FIXED — CoreIR n-ary primitives dropped operands.** `(+ 1 2 3)`→5,
  `(* 2 3 4)`→12, `(- 10 3 2)`→1: `compile_primitive` pushed all operands then
  emitted one binary opcode (pops two), using only the last two. Now any arity
  ≠2 routes through the runtime primitive (`OP_LOAD_VAR`+`OP_CALL`); 2 operands
  keep the fast opcode. Regression tests in `CoreIRPipelineTest`.

- **FIXED — toy VM hung/segfaulted applying a closure from inside a primitive.**
  Surfaced as `(map (lambda ...) ...)` but the real cause was `VM::eval` doing
  `stack.clear()` on entry: a primitive (map/filter/for-each) calling back into
  `eval()` via `Closure::apply` wiped the VM's in-progress frame stack, so the
  step machine spun forever (SIGSEGV via stack overflow at -O0, hang under ASan).
  `eval()` is now re-entrant — it runs the nested expression on top of the
  existing frames down to a depth floor (`run_to`) instead of clearing, keeping
  the outer frames GC-reachable; a re-entrancy guard preserves the top-level
  "clear leftover frames after an exception" behaviour and restores the outer
  accumulator only for nested calls. Covered in the differential suite
  (map/filter with closures); 58 continuation/dynamic-wind tests still pass,
  ASan clean.

- **FIXED — named `let` (`(let loop ...)`) broken in bytecode and toy VM.**
  The shared Expander kept named let as a named-let form and relied on each
  evaluator to implement it (only tree-walk + CoreIR did). The Expander now
  desugars `(let loop ((v i)...) body...)` →
  `(letrec ((loop (lambda (v...) body...))) (loop i...))` and re-expands, so all
  four evaluators get it uniformly (regular letrec already worked everywhere).
  Unmasked in the differential suite. (The per-evaluator named-let handling in
  the tree-walk and CoreIR lowering is now redundant — safe future cleanup.)

- **FIXED — toy VM one-armed `if` / `when` / `unless`.** `step_eval_if` read the
  else branch unconditionally (`first(rest(rest(args)))`), so a two-element
  `(if test then)` did `first(nil)` → "first: cannot take first of an atom!" —
  and did so even when the condition was true, which also broke when/unless
  (they desugar to a one-armed if). Now the else branch is read lazily and a
  missing one yields the unspecified value (nil), matching the tree-walk.
  Unmasked in the differential suite.

- **FIXED — `procedure?` was unbound in all four** (missing primitive). Added as
  `PrimitiveBooleans(PROCEDUREQ)` → `#t` for anything deriving from `Procedure`
  (closure / primitive / continuation); registered and exported from
  `(scheme base)`. Covered in the differential suite.

A second, broader sweep (~77 programs: vectors, do, case/cond edges, values,
variadic lambdas, strings/chars, assq/memq, nested quasiquote) found more:

- **FIXED — `cond` with `=>`, no-body clause, and exhausted no-else.** Each
  evaluator re-implemented `cond`, so they diverged: `=>` worked only in the
  tree-walk; a no-body clause `(cond (42))` returned 42 only in CoreIR (tree-walk
  returned `()` — a `== NULL` vs `nil` bug — bytecode `#t`, toy VM errored); an
  exhausted no-else `(cond (#f 1))` crashed the tree-walk. Fixed by desugaring
  `cond` -> `if`/`let` in the shared Expander (new `expand_cond`, with a `gensym`
  for the `=>` temp) so bytecode/CoreIR/toy-VM get identical R7RS semantics, plus
  a surgical fix to the tree-walk's own `reduce_cond` (no-body returns the test
  value; exhausted returns nil). All four now agree; covered in the differential
  corpus (=>, no-body, exhausted).

- **FIXED — CoreIR variadic / dotted lambda params.** `(lambda args ...)` and
  `(lambda (a . rest) ...)` errored "car of an atom" in CoreIR only (the lowering
  assumed a proper symbol list). `lower_lambda` now parses a bare-symbol tail as
  a rest parameter; the CoreIR `Lambda` carries a `rest` field; and the bytecode
  emitter builds the matching improper/bare formals list so the Closure binds
  extra args into a list (the bytecode VM already handled variadic closures).
  Covered in the differential corpus.

- **FIXED — CoreIR `call-with-values` / `values`.** CoreIR's dedicated
  Values/CallWithValues lowering passed the producer's values to the consumer as
  a single packed object ("too few arguments" / "requires numbers"). Both are
  ordinary registered primitives the bytecode VM already handles, so the special
  dispatch is bypassed and they lower through the normal OP_CALL path. Covered in
  the differential corpus. (The now-dead lower_values / lower_call_with_values /
  makeValues / makeCallWithValues / isCallWithValues paths are safe cleanup.)

Every divergence from both sweeps is now fixed — the differential corpus (~90
programs across arithmetic, predicates, binding forms, lambdas incl. variadic,
lists, higher-order, quote/quasiquote, cond/case, strings, values) is green on
all four evaluators with zero masks.

Feature-gap backlog — being closed (each addition also joins the differential
net, since primitives/derived forms are shared and the tree-walk is the oracle
for "supported"):
- **DONE `boolean=?`** — PrimitiveBooleans(BOOLEANEQ), two-arg; exported.
- **DONE `last-pair`** — PrimitiveLists(LASTPAIR).
- **DONE `letrec*`** — shares letrec's sequential set! desugaring (Expander +
  tree-walk aliased to the letrec path).
- **DONE `let-values` / `let*-values`** — desugar to nested call-with-values
  (Expander `expand_let_values` + a parallel tree-walk branch).

Note on method: primitives register once in the shared environment, so a missing
primitive is absent in all four at once (the toy VM's apparent "support" was its
unbound-name fallback returning garbage). Adding it fixes all four.

## 2.4 Tree-walk / Expander unification (2026-11-15)

The tree-walk (default evaluator) used to reimplement every derived form in
parallel with the shared Expander that the other three evaluators run — the
structural root of the whole divergence class (named let, cond =>, let-values
all drifted because of it). It now desugars those forms (when, unless, and, or,
cond, case, do, let, let*, letrec, letrec*, let-values, let*-values) through the
Expander on encounter and tail-calls the result, exactly as a macro use is
expanded. The Expander is now the single source of truth for derived-form
semantics across ALL evaluators.

- Displacement cache: the tree-walk re-evaluates the same AST repeatedly (loop
  bodies), so naive per-encounter expansion is quadratic. The desugared form is
  cached by rewriting the cons cell in place (deterministic desugaring => pure
  cache); each form expands at most once. Full suite stays at baseline (~28s).
- case-lambda is kept native (procedure constructor like lambda; delegating it
  was ~1000x slower in CaseLambdaTest).
- Removed the ~277 lines of now-dead per-form branches plus the orphaned
  reduce_cond / transform_do_to_letrec helpers.

CoreIR cleanup (2026-11-15): `lower_expr` expands via the shared Expander before
dispatch, so CoreIR's own derived-form handling was redundant too. Removed the
dead named-let desugar from the `let` dispatch, the orphaned `lower_values` /
`lower_call_with_values` (no callers after call-with-values was routed through
the runtime primitive), and the dead CallWithValues tail-call marking. (The
`Values`/`CallWithValues` IR structs + their print/compile/verify handling remain
as harmless, never-instantiated infrastructure.)

- **FIXED — "Unbound variable:" now names the variable** on every evaluator that
  errors. `Environment::lookup`/`set` threw `"Unbound variable: "` with an empty
  name (only the tree-walk's own atom path appended it); both now append
  `get_name(symbol)`, matching the tree-walk.

- **FIXED — toy VM now errors on unbound variables** (symbols), and string
  literals self-evaluate, matching the tree-walk oracle. `step_eval_atom`
  previously looked up strings AND symbols and fell back to returning the symbol
  on failure, silently masking undefined names.
  - Root cause of the earlier "it's load-bearing for continuations" scare: it was
    NOT. The two continuation tests that broke (`ContinuationInClosure`,
    `MultipleContinuationsWithGC`) were **vacuous** — their multi-form setups were
    fed through `read_from_string`, which reads only the FIRST form, so the later
    `define`s (escape, cont1..3, val1..3) were never evaluated. The tests
    "passed" only because `(escape 99)` left 99 in the accumulator via the lenient
    fallback. Fixed `ContinuationTests::eval_vm` to evaluate every top-level form
    (read_program_from_string). With a genuine setup the toy VM's re-entrant
    continuations work correctly AND unbound errors properly (22/22, ASan clean).
  - Confirmed the toy VM genuinely supports re-entrant call/cc (the reason it
    exists): the tree-walk only supports escape continuations and explicitly
    errors ("use the toy VM or bytecode VM") on re-entrant invocation.

- **FIXED — toy VM re-entrant continuations (was escape-only).** Probing showed
  the toy VM did escape continuations correctly but a re-invoked (stored)
  continuation did NOT resume the rest of its computation: a counter/generator
  `(begin ... (define start (call/cc ...)) (set! count ..) (if (< start 3) (k0 ..)
  count))` returned 1 instead of 4 (the bytecode VM returns 4). Root cause:
  `step_eval_args`/`step_eval_begin` short-circuited on a `m_continuation_invoked`
  flag — escape-ONLY behaviour. But `restore_continuation` already replaces the
  whole frame stack with the captured one, so the abandoned frames are gone and
  the remaining (captured) frames must simply resume. Removed both flag checks;
  restored frames now resume via the normal path. Re-entrant continuations work
  (counter -> 4), escape still works (10/11/0/1099), and all three evaluators now
  agree that `(begin (+ 1 2) (call/cc (lambda (k) (k 42))) (+ 3 4))` = 7 (the old
  toy-VM 42 was wrong — begin discards the resumed value and runs the last form).
  `ContinuationWithBegin` corrected to expect 7; added
  `ReentrantCounterInSingleForm`. 23 continuation tests pass, ASan clean.

- **Differential continuation coverage added.** `DifferentialTests.cpp` now has
  escape continuations in the main corpus (all evaluators agree) and a
  `ReentrantContinuations` test comparing bytecode vs toy VM against known values
  (the tree-walk is excluded — it is escape-only and is asserted NOT to produce
  the re-entrant result). This immediately surfaced a **CoreIR gap** (now FIXED).

- **FIXED — CoreIR: a complex form in operand position whose lowering ends in a
  literal.** The call/cc framing was a red herring; the real bug was any `begin`
  (or other non-atomic form) used as an argument, e.g. `(+ 100 (begin 1 999))` ->
  `Unbound variable: $tmp0`. Root cause: `extract_result_var` returned a FRESH,
  UNBOUND temp when a lowered chain's terminal `Return` held a literal (or an If),
  while `inline_stmt_chain` discards the `Return` value — so the invented temp was
  referenced but never bound. Fix: new `terminal_atom(Stmt*)` helper returns the
  terminal `Return`'s actual atom (a Var bound by the preceding Lets, or the Lit
  used directly); the two argument-lowering sites (prim-call and call) use it
  instead of `makeVar(extract_result_var(...))`. Handles non-numeric literal
  terminals too (the old `(+ x 0)` carry-an-atom trick was numeric-only).
  Unmasked in the differential corpus (begin-in-operand cases now ALL four);
  full suite 1298/1298, ASan clean.

### dynamic-wind + continuation: confirmed R7RS 6.10 traces (2026-11-15)

Hand-derived expected traces, then verified per evaluator (log consed, reversed
for chronological order). On a continuation jump: run the `after` thunks of
winders being EXITED (inner->outer), then the `before` thunks of winders being
ENTERED (outer->inner).

| case | shape | expected | lisp | bytecode | toy VM |
|------|-------|----------|------|----------|--------|
| A normal | dynamic-wind, no jump | `(before during after)` | ok | ok | ok |
| B escape | call/cc escapes out of body | `(before d1 after)` | ok | ok | ok |
| C re-entry | re-invoke stored k back INTO extent | `(before after before after)` | errors (escape-only, by design) | `(before after before)` BUG | ok |
| D nested escape | escape out of nested winders | `(b1 b2 a2 a1)` | ok | ok | ok |

- The toy VM is correct for all four (it was purpose-built for this).
- **FIXED [bytecode VM], Case C** — re-entering a dynamic-wind extent now runs
  the trailing `after` at the correct point (`(before after before after)`,
  matching the toy VM and R7RS). Root cause: the old `OP_DYNAMIC_WIND` instruction
  ran the thunk via a *synchronous* C++ call and then called `exit_wind` from that
  C++ frame; on re-entry the continuation resumes the thunk's bytecode frames
  directly, with no C++ dynamic-wind frame left to run `after` (so it fired too
  late, in the eval() wind-cleanup — after the program's final expression already
  observed the log). Fix: **trampolined dynamic-wind**. New `OP_ENTER_WIND` /
  `OP_EXIT_WIND` opcodes; `compile_dynamic_wind` now emits
  `<before> <after> ENTER_WIND <thunk> CALL 0 EXIT_WIND`, so `EXIT_WIND` is a
  bytecode step after a trampolined thunk call — captured by the continuation and
  re-run at the right point on re-entry. `OP_DYNAMIC_WIND`/`DynamicWindInsn` left
  registered but unused. Confirmed traces A/B/C/D all pass on bytecode; return
  value preserved. Added to the differential `ReentrantContinuations` corpus
  (bytecode vs toy VM agree). Full suite 1298/1298, ASan clean.

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
