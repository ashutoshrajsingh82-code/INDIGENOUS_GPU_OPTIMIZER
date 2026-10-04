#!/usr/bin/env python3
"""Run the Phase 2 solver against a curated Netlib LP subset and HiGHS."""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent
NETLIB_DIR = ROOT / "benchmarks" / "netlib"
MANIFEST = NETLIB_DIR / "manifest.txt"
DEFAULT_SOLVER = ROOT / "build" / "Release" / "solver_phase2_cli.exe"
DEFAULT_HIGHS_CANDIDATES = [
    ROOT.parent / "HiGHS" / "build" / "Release" / "bin" / "highs.exe",
    ROOT.parent.parent / "HiGHS" / "build" / "Release" / "bin" / "highs.exe",
]
NETLIB_BASE = "https://www.netlib.org/lp/data/"

OBJECTIVE_RE = re.compile(
    r"^Objective:\s*([-+]?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?)\s*$",
    re.MULTILINE,
)
HIGHS_OBJECTIVE_RE = re.compile(
    r"Objective value\s*:\s*([-+]?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?)"
)
ITERATIONS_RE = re.compile(r"^Iterations:\s*(\d+)\s*$", re.MULTILINE)
STATUS_RE = re.compile(r"^Status:\s*(\S+)", re.MULTILINE)
CERTIFICATE_RE = re.compile(r"^Certificate:\s*(\S+)", re.MULTILINE)
HIGHS_ITERATIONS_RE = re.compile(r"Simplex\s+iterations:\s*(\d+)")
HIGHS_STATUS_RE = re.compile(r"Model status\s*:\s*(\S+)")


def find_highs() -> Path | None:
    env_highs = os.environ.get("HIGHS_EXE")
    if env_highs and Path(env_highs).expanduser().is_file():
        return Path(env_highs).expanduser()
    path_highs = shutil.which("highs")
    if path_highs:
        return Path(path_highs)
    for candidate in DEFAULT_HIGHS_CANDIDATES:
        if candidate.is_file():
            return candidate
    return None


def run_command(command: list[str]) -> tuple[int, str]:
    try:
        result = subprocess.run(
            command,
            cwd=ROOT,
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


def model_names() -> list[str]:
    if not MANIFEST.is_file():
        return []
    return [
        line.strip()
        for line in MANIFEST.read_text(encoding="utf-8").splitlines()
        if line.strip() and not line.lstrip().startswith("#")
    ]


def ensure_models(names: list[str]) -> list[Path]:
    NETLIB_DIR.mkdir(parents=True, exist_ok=True)
    models: list[Path] = []
    for name in names:
        destination = NETLIB_DIR / (name + ".mps")
        if destination.is_file() and destination.stat().st_size > 0:
            models.append(destination)
            continue

        url = NETLIB_BASE + name
        print(f"[DOWNLOAD] {name} <- {url}")
        try:
            urllib.request.urlretrieve(url, destination)
            if destination.is_file() and destination.stat().st_size > 0:
                models.append(destination)
        except Exception as exc:
            print(f"[FAIL] {name}: download failed: {exc}")
            if destination.exists():
                destination.unlink()
    return models


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--solver", type=Path, default=DEFAULT_SOLVER)
    parser.add_argument("--highs", type=Path, default=None)
    parser.add_argument("--tolerance", type=float, default=1e-7)
    parser.add_argument(
        "--download",
        action="store_true",
        help="Download missing models listed in benchmarks/netlib/manifest.txt.",
    )
    parser.add_argument(
        "models",
        nargs="*",
        help="Optional Netlib model names. Defaults to the manifest subset.",
    )
    args = parser.parse_args()

    solver = args.solver.expanduser()
    highs = args.highs.expanduser() if args.highs else find_highs()
    names = args.models or model_names()

    if not names:
        print(f"ERROR: no Netlib models listed in {MANIFEST}")
        return 2

    if args.download:
        models = ensure_models(names)
    else:
        models = [NETLIB_DIR / (name + ".mps") for name in names]

    if not solver.is_file():
        print(f"ERROR: solver not found: {solver}")
        return 2
    if highs is None or not highs.is_file():
        print("ERROR: HiGHS not found; use --highs or HIGHS_EXE")
        return 2

    print("Indigenous GPU Optimizer vs HiGHS — Netlib subset")
    print(f"Solver: {solver}")
    print(f"HiGHS:  {highs}")
    print()

    failures = 0
    executed = 0

    for model in models:
        if not model.is_file():
            print(f"[SKIP] {model.name}: missing (run with --download)")
            failures += 1
            continue

        # Netlib classic LP files commonly have no extension, but the solver
        # CLI determines the format from file content/path support.
        solver_code, solver_out = run_command(
            [str(solver), "solve", str(model), "--max-iters", "100000"]
        )
        highs_code, highs_out = run_command([str(highs), str(model)])

        solver_status = extract(STATUS_RE, solver_out) or "UNKNOWN"
        solver_obj = extract(OBJECTIVE_RE, solver_out)
        iterations = extract(ITERATIONS_RE, solver_out) or "-"
        certificate = extract(CERTIFICATE_RE, solver_out) or "-"
        highs_status = extract(HIGHS_STATUS_RE, highs_out) or "UNKNOWN"
        highs_obj = extract(HIGHS_OBJECTIVE_RE, highs_out)
        highs_iterations = extract(HIGHS_ITERATIONS_RE, highs_out) or "-"

        if solver_obj is not None and highs_obj is not None:
            difference = abs(float(solver_obj) - float(highs_obj))
            difference_text = f"{difference:.3e}"
        else:
            difference = float("inf")
            difference_text = "n/a"

        ok = (
            solver_code == 0
            and highs_code == 0
            and solver_status == "OPTIMAL"
            and highs_status == "Optimal"
            and solver_obj is not None
            and highs_obj is not None
            and difference <= args.tolerance
            and certificate == "PASS"
        )

        print(
            f"[{'PASS' if ok else 'FAIL'}] {model.name}: "
            f"indigenous={solver_obj or 'n/a'} "
            f"highs={highs_obj or 'n/a'} "
            f"diff={difference_text} "
            f"iterations={iterations} "
            f"highs_iterations={highs_iterations} "
            f"certificate={certificate}"
        )

        executed += 1
        if not ok:
            failures += 1
            print("--- Indigenous solver output ---")
            print(solver_out.rstrip())
            print("--- HiGHS output ---")
            print(highs_out.rstrip())
            print()

    print()
    print(f"Models:    {executed}")
    print(f"Passed:    {executed - failures}")
    print(f"Failed:    {failures}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
