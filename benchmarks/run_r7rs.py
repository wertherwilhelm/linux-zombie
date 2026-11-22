#!/usr/bin/env python3
"""Run an external r7rs-benchmarks checkout against an unchanged evaluator.

Every job gets its own process and copied data fixtures. Upstream Scheme sources
and correctness predicates are kept intact. Smoke inputs are explicitly reduced;
one-iteration changes only the repetition count; original changes nothing.
"""
import argparse
import collections
import csv
import datetime
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import re
import resource
import shutil
import signal
import subprocess
import tempfile
import time

SMOKE_INPUTS = {
    "fib": "1\n20\n6765\n",
    "fibfp": "1\n20.0\n6765.0\n",
    "fibc": "1\n10\n55\n",
    "sum": "1\n1000\n500500\n",
    "sumfp": "1\n1000.0\n500500.0\n",
    "ack": "1\n3\n4\n125\n",
    "array1": "1\n1000\n1000\n",
    "diviter": "1\n100\n50\n",
    "divrec": "1\n100\n50\n",
    "tak": "1\n12\n6\n0\n1\n",
    "cpstak": "1\n12\n6\n0\n1\n",
    "ctak": "1\n12\n6\n0\n1\n",
    "nqueens": "1\n6\n4\n",
    "quicksort": "1\n100\n10000\nignored\n",
    "fft": "1\n64\n0.0\n0.0\n",
}
CSV_FIELDS = ["benchmark", "evaluator", "profile", "repeat", "status", "benchmark_name",
              "benchmark_seconds", "process_seconds", "returncode", "detail"]
MARKER = "+!CSVLINE!+"


def git_value(root, *args):
    result = subprocess.run(["git", "-C", str(root), *args], text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    return result.stdout.strip() if result.returncode == 0 else None


def names_from_suite(suite):
    groups = re.findall(r'^([A-Z_]+_BENCHMARKS)="([a-zA-Z0-9 ]+)"$',
                        (suite / "bench").read_text(), re.MULTILINE)
    return list(dict.fromkeys(name for _, names in groups for name in names.split()))


def one_iteration(text):
    # A comment can precede the first count datum (mperm.input does this).
    result, replacements = re.subn(r"(?m)^([ \t]*)[0-9]+(?=\s|;|$)", r"\g<1>1", text, count=1)
    if replacements != 1:
        raise ValueError("could not identify benchmark repetition count")
    return result


def verdict(returncode, stdout, stderr):
    row = None
    for line in stdout.splitlines():
        if line.startswith(MARKER):
            parsed = next(csv.reader([line[len(MARKER):]]))
            if len(parsed) == 3:
                row = parsed
    if returncode != 0:
        detail = stderr.strip() or stdout.strip()
        if returncode < 0:
            try:
                detail = signal.Signals(-returncode).name + ": " + detail
            except ValueError:
                pass
            return "crashed", "", "", detail[-1600:]
        return "error", "", "", detail[-1600:]
    if row is None:
        return "no_result", "", "", "No upstream correctness/timing marker was produced"
    if row[2] == "INCORRECT":
        return "incorrect", row[1], "", stdout.strip()[-1600:]
    try:
        seconds = float(row[2])
        if not math.isfinite(seconds) or seconds < 0:
            raise ValueError()
    except ValueError:
        return "invalid_result", row[1], "", row[2]
    if stderr.strip():
        return "error", row[1], "", stderr.strip()[-1600:]
    return "passed", row[1], seconds, ""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--suite", type=Path, required=True)
    parser.add_argument("--exe", type=Path, default=Path("build-release/zombie-scheme"))
    parser.add_argument("--evaluators", nargs="+", choices=["lisp", "bytecode", "coreir", "toyvm"],
                        default=["lisp", "bytecode", "coreir"])
    parser.add_argument("--benchmarks", nargs="+", default=["all"])
    parser.add_argument("--profile", choices=["smoke", "one-iteration", "original"], default="smoke")
    parser.add_argument("--timeout", type=float, default=10.0, help="wall seconds allowed per process")
    parser.add_argument("--memory-mb", type=int, default=1024, help="per-process address-space limit")
    parser.add_argument("--repeats", type=int, default=1, help="fresh process runs per case")
    parser.add_argument("--output", type=Path, required=True, help="new result directory")
    args = parser.parse_args()
    args.suite, args.exe, args.output = args.suite.resolve(), args.exe.resolve(), args.output.resolve()
    if args.timeout <= 0 or args.memory_mb <= 0 or args.repeats <= 0:
        parser.error("timeout, memory, and repeats must be positive")
    if not args.exe.is_file():
        parser.error(f"runner does not exist: {args.exe}")
    for required in ["bench", "src/common.scm", "src/common-postlude.scm", "inputs"]:
        if not (args.suite / required).exists():
            parser.error(f"not an r7rs-benchmarks checkout: missing {required}")
    available = names_from_suite(args.suite)
    if args.benchmarks == ["all"]:
        names = list(SMOKE_INPUTS) if args.profile == "smoke" else available
    else:
        names = list(dict.fromkeys(args.benchmarks))
    for name in names:
        if name not in available or not (args.suite / "src" / f"{name}.scm").is_file():
            parser.error(f"unknown upstream benchmark: {name}")
        if args.profile == "smoke" and name not in SMOKE_INPUTS:
            parser.error(f"no reduced smoke input for {name}; use one-iteration or original")
    if args.output.exists():
        parser.error("output already exists; choose a new directory to preserve prior results")
    args.output.mkdir(parents=True)
    (args.output / "logs").mkdir()
    (args.output / "programs").mkdir()
    (args.output / "inputs").mkdir()
    root = Path(__file__).resolve().parent.parent
    metadata = {
        "started_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "source_commit": git_value(root, "rev-parse", "HEAD"),
        "source_branch": git_value(root, "branch", "--show-current"),
        "source_status": git_value(root, "status", "--short"),
        "suite_commit": git_value(args.suite, "rev-parse", "HEAD"),
        "suite_status": git_value(args.suite, "status", "--short"),
        "executable": str(args.exe), "executable_sha256": hashlib.sha256(args.exe.read_bytes()).hexdigest(),
        "platform": platform.platform(), "machine": platform.machine(),
        "suite": str(args.suite), "profile": args.profile, "evaluators": args.evaluators,
        "benchmarks": names, "timeout_seconds": args.timeout, "memory_mb": args.memory_mb,
        "stdin_transport": "regular file (matches upstream bench redirection)",
        "repeats": args.repeats, "timing_adapter": "monotonic microseconds; secondary POSIX wall time",
        "notes": "Reduced profiles are compatibility checks, not comparable to published full-input timings."
    }
    cache = args.exe.parent / "CMakeCache.txt"
    if cache.is_file():
        build_type = re.search(r"^CMAKE_BUILD_TYPE:STRING=(.*)$", cache.read_text(), re.MULTILINE)
        metadata["build_type"] = build_type.group(1) if build_type else "unknown"
    (args.output / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n")
    common = (args.suite / "src/common.scm").read_text()
    postlude = (args.suite / "src/common-postlude.scm").read_text()
    records = []

    def limits():
        memory = args.memory_mb * 1024 * 1024
        resource.setrlimit(resource.RLIMIT_AS, (memory, memory))
        cpu = max(1, math.ceil(args.timeout) + 1)
        resource.setrlimit(resource.RLIMIT_CPU, (cpu, cpu))
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))

    # Copy external fixtures once, then recreate an isolated writable tree per
    # job. Benchmarks such as slatex can write next to their input files.
    with tempfile.TemporaryDirectory(prefix="zombie-r7rs-") as temporary:
        temporary = Path(temporary)
        fixtures = temporary / "fixtures"
        shutil.copytree(args.suite / "inputs", fixtures)
        with (args.output / "results.csv").open("w", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=CSV_FIELDS)
            writer.writeheader()
            for name in names:
                original = (args.suite / "inputs" / f"{name}.input").read_text()
                data = (SMOKE_INPUTS[name] if args.profile == "smoke" else
                        one_iteration(original) if args.profile == "one-iteration" and name != "equal"
                        else original)
                # equal.scm already hardcodes one repetition; its first datum
                # is a problem parameter, not a repetition count. Keep it intact.
                (args.output / "inputs" / f"{name}.input").write_text(data)
                source = (args.suite / "src" / f"{name}.scm").read_text()
                for evaluator in args.evaluators:
                    program = args.output / "programs" / f"{name}-{evaluator}.scm"
                    implementation = f'(define (this-scheme-implementation-name) "ZombieScheme-{evaluator}")\n'
                    program.write_text("\n".join([source, common, implementation, postlude]))
                    for repeat in range(1, args.repeats + 1):
                        with tempfile.TemporaryDirectory(prefix="job-", dir=temporary) as work:
                            work = Path(work)
                            shutil.copytree(fixtures, work / "inputs")
                            (work / "outputs").mkdir()
                            start = time.perf_counter()
                            try:
                                # Upstream uses `< inputs/name.input`, not a pipe.
                                # Keep stdin seekable: ZombieScheme's stream reader
                                # takes a different path for interactive/pipe input.
                                with (args.output / "inputs" / f"{name}.input").open() as input_stream:
                                    process = subprocess.run([str(args.exe), "--evaluator", evaluator,
                                                              "--benchmark-time", str(program)],
                                                             stdin=input_stream, text=True, stdout=subprocess.PIPE,
                                                             stderr=subprocess.PIPE, cwd=work, timeout=args.timeout,
                                                             preexec_fn=limits)
                                stdout, stderr, returncode = process.stdout, process.stderr, process.returncode
                                status, bench_name, seconds, detail = verdict(returncode, stdout, stderr)
                            except subprocess.TimeoutExpired as error:
                                stdout = error.stdout or b""
                                stderr = error.stderr or b""
                                if isinstance(stdout, bytes): stdout = stdout.decode(errors="replace")
                                if isinstance(stderr, bytes): stderr = stderr.decode(errors="replace")
                                returncode, status, bench_name, seconds = None, "timeout", "", ""
                                detail = f"Exceeded {args.timeout:g}s wall limit"
                            elapsed = time.perf_counter() - start
                        stem = f"{name}-{evaluator}-{repeat}"
                        (args.output / "logs" / f"{stem}.stdout").write_text(stdout)
                        (args.output / "logs" / f"{stem}.stderr").write_text(stderr)
                        record = dict(zip(CSV_FIELDS, [name, evaluator, args.profile, repeat, status, bench_name,
                                                      seconds, elapsed, returncode, detail]))
                        records.append(record)
                        writer.writerow(record)
                        stream.flush()
                        (args.output / "results.json").write_text(json.dumps(records, indent=2) + "\n")
                        print(f"{name:12} {evaluator:8} {status:14} "
                              f"benchmark={seconds if seconds != '' else '-'}s wall={elapsed:.3f}s", flush=True)
    summary = {e: dict(collections.Counter(r["status"] for r in records if r["evaluator"] == e))
               for e in args.evaluators}
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary, indent=2))
    return 0 if all(r["status"] == "passed" for r in records) else 1


if __name__ == "__main__":
    raise SystemExit(main())
