#!/usr/bin/env python3
"""Run the Phase 2 solver against a curated Netlib LP subset and HiGHS."""

from __future__ import annotations

import argparse
import gzip
import os
import re
import shutil
import subprocess
import sys
import time
import csv
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
NETLIB_BASE = "https://raw.githubusercontent.com/coin-or-tools/Data-Netlib/master/"

OBJECTIVE_RE = re.compile(
    r"^Objective:\s*([-+]?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?)\s*$",
    re.MULTILINE,
)
HIGHS_OBJECTIVE_RE = re.compile(
    r"Objective value\s*:\s*([-+]?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?)"
)
ITERATIONS_RE = re.compile(r"^Iterations:\s*(\d+)\s*$", re.MULTILINE)
STATUS_RE = re.compile(r"Status:\s*(\S+)")
CERTIFICATE_RE = re.compile(r"^Certificate:\s*(\S+)\s*$", re.MULTILINE)
HIGHS_ITERATIONS_RE = re.compile(r"Simplex\s+iterations:\s*(\d+)")
HIGHS_STATUS_RE = re.compile(r"Model status\s*:\s*(\S+)")
ANSI_RE = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")

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
    normalized = ANSI_RE.sub("", text).replace("\x00", "")
    match = pattern.search(normalized)
    return match.group(1) if match else None

def model_names() -> list[str]:
    if not MANIFEST.is_file():
        return []
    return [
        line.strip()
        for line in MANIFEST.read_text(encoding="utf-8").splitlines()
        if line.strip() and not line.lstrip().startswith("#")
    ]

def is_mps_file(path: Path) -> bool:
    """Return True when a file starts like a plain-text MPS model."""
    if not path.is_file() or path.stat().st_size == 0:
        return False
    try:
        with path.open("r", encoding="ascii", errors="ignore") as handle:
            header = handle.read(4096)
    except OSError:
        return False
    return "NAME" in header and "ROWS" in header and "COLUMNS" in header

def ensure_models(names: list[str]) -> list[Path]:
    NETLIB_DIR.mkdir(parents=True, exist_ok=True)
    models: list[Path] = []
    for name in names:
        destination = NETLIB_DIR / (name + ".mps")
        if is_mps_file(destination):
            models.append(destination)
            continue

        compressed = NETLIB_DIR / (name + ".mps.gz")
        url = NETLIB_BASE + name.lower() + ".mps.gz"
        print(f"[DOWNLOAD] {name} <- {url}")
        try:
            urllib.request.urlretrieve(url, compressed)
            with gzip.open(compressed, "rb") as source, destination.open("wb") as target:
                shutil.copyfileobj(source, target)
            compressed.unlink()
            if is_mps_file(destination):
                models.append(destination)
            else:
                raise ValueError("downloaded file is not plain-text MPS")
        except Exception as exc:
            print(f"[FAIL] {name}: download/conversion failed: {exc}")
            for path in (compressed, destination):
                if path.exists():
                    path.unlink()
    return models

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--solver", type=Path, default=DEFAULT_SOLVER)
    parser.add_argument("--highs", type=Path, default=None)
    parser.add_argument(
        "--tolerance",
        type=float,
        default=1e-7,
        help="Absolute objective tolerance.",
    )
    parser.add_argument(
        "--relative-tolerance",
        type=float,
        default=1e-9,
        help="Relative objective tolerance used with --tolerance.",
    )
    parser.add_argument(
        "--csv",
        type=Path,
        default=None,
        help="Write per-model timing/statistics to a CSV file.",
    )
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
    print(
        f"Objective tolerance: abs={args.tolerance:.3e}, "
        f"rel={args.relative_tolerance:.3e}"
    )
    print()

    failures = 0
    executed = 0
    rows: list[dict[str, str]] = []

    for model in models:
        if not model.is_file():
            print(f"[SKIP] {model.name}: missing (run with --download)")
            failures += 1
            continue

        solver_start = time.perf_counter()
        solver_code, solver_out = run_command(
            [str(solver), "solve", str(model), "--max-iters", "100000"]
        )
        solver_ms = (time.perf_counter() - solver_start) * 1000.0

        highs_start = time.perf_counter()
        highs_code, highs_out = run_command([str(highs), str(model)])
        highs_ms = (time.perf_counter() - highs_start) * 1000.0

        solver_status = extract(STATUS_RE, solver_out) or "UNKNOWN"
        solver_obj = extract(OBJECTIVE_RE, solver_out)
        iterations = extract(ITERATIONS_RE, solver_out) or "-"
        certificate = extract(CERTIFICATE_RE, solver_out) or "-"
        highs_status = extract(HIGHS_STATUS_RE, highs_out) or "UNKNOWN"
        highs_obj = extract(HIGHS_OBJECTIVE_RE, highs_out)
        highs_iterations = extract(HIGHS_ITERATIONS_RE, highs_out) or "-"

        if solver_obj is not None and highs_obj is not None:
            solver_value = float(solver_obj)
            highs_value = float(highs_obj)
            difference = abs(solver_value - highs_value)
            comparison_limit = max(
                args.tolerance,
                args.relative_tolerance
                * max(1.0, abs(solver_value), abs(highs_value)),
            )
            difference_text = f"{difference:.3e}"
            comparison_limit_text = f"{comparison_limit:.3e}"
        else:
            difference = float("inf")
            comparison_limit = 0.0
            difference_text = "n/a"
            comparison_limit_text = "n/a"

        checks = {
            "solver_rc": solver_code == 0,
            "highs_rc": highs_code == 0,
            "solver_status": solver_status == "OPTIMAL",
            "highs_status": highs_status == "Optimal",
            "solver_objective": solver_obj is not None,
            "highs_objective": highs_obj is not None,
            "objective_tolerance": difference <= comparison_limit,
            "certificate": certificate == "PASS",
        }
        ok = all(checks.values())

        failed_checks = ",".join(
            name for name, passed in checks.items() if not passed
        ) or "-"

        print(
            f"[{'PASS' if ok else 'FAIL'}] {model.name}: "
            f"indigenous={solver_obj or 'n/a'} "
            f"highs={highs_obj or 'n/a'} "
            f"diff={difference_text} "
            f"limit={comparison_limit_text} "
            f"iterations={iterations} "
            f"highs_iterations={highs_iterations} "
            f"solver_ms={solver_ms:.3f} "
            f"highs_ms={highs_ms:.3f} "
            f"certificate={certificate} "
            f"solver_rc={solver_code} "
            f"highs_rc={highs_code} "
            f"failed_checks={failed_checks}"
        )

        rows.append({
            "model": model.stem,
            "status": "PASS" if ok else "FAIL",
            "solver_status": solver_status,
            "highs_status": highs_status,
            "solver_objective": solver_obj or "",
            "highs_objective": highs_obj or "",
            "objective_difference": f"{difference:.12g}" if difference != float("inf") else "",
            "objective_limit": f"{comparison_limit:.12g}",
            "solver_iterations": iterations,
            "highs_iterations": highs_iterations,
            "solver_ms": f"{solver_ms:.3f}",
            "highs_ms": f"{highs_ms:.3f}",
            "certificate": certificate,
            "solver_rc": str(solver_code),
            "highs_rc": str(highs_code),
            "failed_checks": failed_checks,
        })

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

    if rows:
        solver_times = [float(row["solver_ms"]) for row in rows]
        highs_times = [float(row["highs_ms"]) for row in rows]
        solver_iters = [int(row["solver_iterations"]) for row in rows if row["solver_iterations"].isdigit()]
        highs_iters = [int(row["highs_iterations"]) for row in rows if row["highs_iterations"].isdigit()]
        print(
            "Solver timing (ms): "
            f"total={sum(solver_times):.3f} "
            f"avg={sum(solver_times)/len(solver_times):.3f} "
            f"min={min(solver_times):.3f} "
            f"max={max(solver_times):.3f}"
        )
        print(
            "HiGHS timing (ms):  "
            f"total={sum(highs_times):.3f} "
            f"avg={sum(highs_times)/len(highs_times):.3f} "
            f"min={min(highs_times):.3f} "
            f"max={max(highs_times):.3f}"
        )
        if solver_iters:
            print(
                "Solver iterations:  "
                f"total={sum(solver_iters)} "
                f"avg={sum(solver_iters)/len(solver_iters):.1f} "
                f"min={min(solver_iters)} "
                f"max={max(solver_iters)}"
            )
        if highs_iters:
            print(
                "HiGHS iterations:   "
                f"total={sum(highs_iters)} "
                f"avg={sum(highs_iters)/len(highs_iters):.1f} "
                f"min={min(highs_iters)} "
                f"max={max(highs_iters)}"
            )

    if args.csv and rows:
        args.csv.parent.mkdir(parents=True, exist_ok=True)
        fieldnames = list(rows[0].keys())
        with args.csv.open("w", newline="", encoding="utf-8") as handle:
            writer = csv.DictWriter(handle, fieldnames=fieldnames)
            writer.writeheader()
            writer.writerows(rows)
        print(f"CSV:       {args.csv}")

    return 1 if failures else 0

if __name__ == "__main__":
    sys.exit(main())
