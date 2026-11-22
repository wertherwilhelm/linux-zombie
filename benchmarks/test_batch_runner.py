#!/usr/bin/env python3
"""Integration checks: file input must not consume benchmark data on stdin."""
import argparse
from pathlib import Path
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument("--exe", type=Path, required=True)
a = p.parse_args()
exe = a.exe.resolve()
with tempfile.TemporaryDirectory(prefix="zombie-batch-check-") as temporary:
    source = Path(temporary) / "program.scm"
    for evaluator in ["lisp", "bytecode", "coreir", "toyvm"]:
        source.write_text('(import (scheme base) (scheme read) (scheme write))\n(write (+ (read) (read)))\n')
        r = subprocess.run([str(exe), "--evaluator", evaluator, str(source)], input="19\n23\n",
                           text=True, capture_output=True, timeout=10)
        assert r.returncode == 0 and not r.stderr and r.stdout in {"42", "42.0"}, (evaluator, r)
        data = Path(temporary) / "data.input"
        data.write_text("19\n23\n")
        with data.open() as stdin:
            r = subprocess.run([str(exe), "--evaluator", evaluator, str(source)], stdin=stdin,
                               text=True, capture_output=True, timeout=10)
        assert (r.returncode, r.stdout, r.stderr) == (0, "42", ""), (evaluator, r)
        source.write_text('(import (scheme time))\n(write (and (integer? (current-jiffy)) (> (jiffies-per-second) 0)))\n')
        r = subprocess.run([str(exe), "--evaluator", evaluator, "--benchmark-time", str(source)],
                           text=True, capture_output=True, timeout=10)
        assert (r.returncode, r.stdout, r.stderr) == (0, "#t", ""), (evaluator, r)
    for text in ['(import (missing library))', '(+ 1 "bad")', '(define broken']:
        source.write_text(text)
        r = subprocess.run([str(exe), str(source)], text=True, capture_output=True, timeout=10)
        assert r.returncode != 0 and r.stderr, (text, r)
    source.write_text("")
    r = subprocess.run([str(exe), str(source)], text=True, capture_output=True, timeout=10)
    assert (r.returncode, r.stdout, r.stderr) == (0, "", ""), r
    source.write_text('(write (command-line))')
    r = subprocess.run([str(exe), str(source), 'hello', 'two words'], text=True, capture_output=True, timeout=10)
    assert r.returncode == 0 and '"hello" "two words"' in r.stdout and not r.stderr, r
    source.write_text('(import (scheme time))')
    r = subprocess.run([str(exe), str(source)], text=True, capture_output=True, timeout=10)
    assert r.returncode != 0, r  # The adapter must be explicitly enabled.
    r = subprocess.run([str(exe), '--evaluator', 'unknown', str(source)], text=True, capture_output=True)
    assert r.returncode == 2, r
print('PASS: four evaluators, separate stdin, quiet output, clock adapter opt-in, error exit codes')
