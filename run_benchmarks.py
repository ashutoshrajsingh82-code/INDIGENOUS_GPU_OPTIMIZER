#!/usr/bin/env python3
"""Compare the Phase 2 solver against HiGHS on bundled LP benchmarks."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent
BENCHMARK_DIR = ROOT / "benchmarks"
DEFAULT_SOLVER = ROOT / "build" / "Release" / "solver_phase2_cli.exe"
DEFAULT_HIGHS = ROOT.parent / "HiGHS" / "build" / "Release" / "bin" / "highs.exe"

OBJECTIVE_RE = re.compile(r"^Objective:\s*([-+]?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?)\s*$", re.MULTILINE)
HIGHS_OBJECTIVE_RE = re.compile(
    r"Objective value\s*:\s*([-+]?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?)"
)
ITERATIONS_RE = re.compile(r"^Iterations:\s*(\d+)\s*$", re.MULTILINE)
STATUS_RE = re.compile(r"^Status:\s*(\S+)", re.MULTILINE)
CERTIFICATE_RE = re.compile(r"^Certificate:\s*(\S+)", re.MULTILINE)
HIGHS_ITERATIONS_RE = re.compile(r"Simplex\s+iterations:\s*(\d+)")
HIGHS_STATUS_RE = re.compile(r"Model status\s*:\s*(\S+)")


def run_command(command: list[str], cwd: Path) -> tuple[int, str]:
    try:
        result = subprocess.run(
            command,
            cwd=cwd,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
    except OSError as exc:
        return 127, f"ERROR: {exc}"
    return result.returncode, result.stdout


def extract(pattern: re.Pattern[str], text: str) -> str | None:
    match = pattern.search(text)
    return match.group(1) if match else None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--solver", type=Path, default=DEFAULT_SOLVER)
    parser.add_argument("--highs", type=Path, default=DEFAULT_HIGHS)
    parser.add_argument("--tolerance", type=float, default=1e-7)
    parser.add_argument(
        "models",
        nargs="*",
        type=Path,
        help="Optional LP files. Defaults to every .lp file in benchmarks/.",
    )
    args = parser.parse_args()

    solver = args.solver.expanduser()
    highs = args.highs.expanduser()
    models = [p if p.is_absolute() else ROOT / p for p in args.models]
    if not models:
        models = sorted(BENCHMARK_DIR.glob("*.lp"))

    missing = [str(p) for p in (solver, highs) if not p.is_file()]
    if missing:
        print("ERROR: missing executable(s):")
        for path in missing:
            print(f"  {path}")
        print("Use --solver and --highs to specify their locations.")
        return 2

    if not models:
        print(f"ERROR: no LP benchmarks found in {BENCHMARK_DIR}")
        return 2

    print("Indigenous GPU Optimizer vs HiGHS")
    print(f"Solver: {solver}")
    print(f"HiGHS:  {highs}")
    print()

    failures = 0
    rows: list[tuple[str, str, str, str, str, str]] = []

    for model in models:
        if not model.is_file():
            print(f"[FAIL] {model}: file not found")
            failures += 1
            continue

        solver_code, solver_out = run_command(
            [str(solver), "solve", str(model), "--max-iters", "100000"],
            ROOT,
        )
        highs_code, highs_out = run_command([str(highs), str(model)], ROOT)

        solver_status = extract(STATUS_RE, solver_out) or "UNKNOWN"
        solver_obj = extract(OBJECTIVE_RE, solver_out)
        iterations = extract(ITERATIONS_RE, solver_out) or "-"
        certificate = extract(CERTIFICATE_RE, solver_out) or "-"
        highs_status = extract(HIGHS_STATUS_RE, highs_out) or "UNKNOWN"
        highs_obj = extract(HIGHS_OBJECTIVE_RE, highs_out)
        highs_iterations = extract(HIGHS_ITERATIONS_RE, highs_out) or "-"

        ok = (
            solver_code == 0
            and highs_code == 0
            and solver_status == "OPTIMAL"
            and highs_status == "Optimal"
            and solver_obj is not None
            and highs_obj is not None
            and abs(float(solver_obj) - float(highs_obj)) <= args.tolerance
            and certificate == "PASS"
        )

        if solver_obj is not None and highs_obj is not None:
            difference = f"{abs(float(solver_obj) - float(highs_obj)):.3e}"
        else:
            difference = "n/a"

        rows.append(
            (
                model.name,
                solver_status,
                solver_obj or "n/a",
                highs_obj or "n/a",
                difference,
                iterations,
            )
        )

        print(
            f"[{'PASS' if ok else 'FAIL'}] {model.name}: "
            f"indigenous={solver_obj or 'n/a'} "
            f"highs={highs_obj or 'n/a'} "
            f"diff={difference} "
            f"iterations={iterations} "
            f"highs_iterations={highs_iterations} "
            f"certificate={certificate}"
        )

        if not ok:
            failures += 1
            print("--- Indigenous solver output ---")
            print(solver_out.rstrip())
            print("--- HiGHS output ---")
            print(highs_out.rstrip())
            print()

    print()
    print(f"Benchmarks: {len(rows)}")
    print(f"Passed:     {len(rows) - failures}")
    print(f"Failed:     {failures}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
