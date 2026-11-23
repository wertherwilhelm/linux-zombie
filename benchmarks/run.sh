#!/usr/bin/env bash
# R7RS-benchmark gap-finder. Assembles each benchmark (<name>.scm + common.scm +
# a (run-benchmark) postlude), runs it on each evaluator with its input, and
# classifies the outcome. Programs come from the ecraven r7rs-benchmarks checkout.
#
# Usage: benchmarks/run.sh [timeout_seconds] [benchmark ...]
#   With no benchmark names, runs every benchmarks/inputs/*.input.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SUITE="${R7RS_BENCH_SRC:-/mnt/c/work/r7rs-benchmarks/src}"
# File benchmarks (cat, ray, …) use paths relative to the suite root (inputs/,
# outputs/); run each program with that as the working directory.
BENCH_ROOT="$(dirname "$SUITE")"
mkdir -p "$BENCH_ROOT/outputs" 2>/dev/null || true
RUNNER="$ROOT/build/zombie-scheme"
COMMON="$SUITE/common.scm"
TIMEOUT="${1:-8}"; shift || true
EVALS=(lisp bytecode coreir toyvm)

names=("$@")
if [ ${#names[@]} -eq 0 ]; then
  for f in "$ROOT"/benchmarks/inputs/*.input; do names+=("$(basename "$f" .input)"); done
fi

printf 'benchmark,%s\n' "$(IFS=,; echo "${EVALS[*]}")"
for name in "${names[@]}"; do
  src="$SUITE/$name.scm"; input="$ROOT/benchmarks/inputs/$name.input"
  [ -f "$src" ] && [ -f "$input" ] || { printf '%s,MISSING\n' "$name"; continue; }
  prog="$(mktemp)"; cat "$src" "$COMMON" > "$prog"; echo '(run-benchmark)' >> "$prog"
  row="$name"
  for ev in "${EVALS[@]}"; do
    out="$(cd "$BENCH_ROOT" && timeout "$TIMEOUT" "$RUNNER" --benchmark-time --evaluator "$ev" "$prog" < "$input" 2>"$prog.err")"
    code=$?
    if   [ $code -eq 124 ]; then r=timeout
    elif [ $code -ge 128 ]; then r="crash($((code-128)))"
    elif [ $code -eq 0 ]; then
      if echo "$out" | grep -q INCORRECT; then r=wrong; else r=passed; fi
    else
      r="error: $(head -1 "$prog.err" | sed 's/.*Evaluation error: //; s/.*error: //' | cut -c1-60)"
    fi
    row="$row|$r"
  done
  printf '%s\n' "$row"
  rm -f "$prog" "$prog.err"
done
