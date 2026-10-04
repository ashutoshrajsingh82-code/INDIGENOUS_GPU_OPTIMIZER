# LP Benchmarks

This directory contains LP instances used to compare the indigenous Phase 2 revised-simplex solver against HiGHS.

## Included models

- `test_small.lp`: 2 variables, 3 constraints. Expected objective: -6.
- `test_medium.lp`: 3 variables, 3 constraints. Expected objective: -34.
- `test_bounds.lp`: 2 variables, 3 constraints. Exercises several binding constraints. Expected objective: -22.8.
- `test_degenerate.lp`: 2 variables, 4 constraints. Contains redundant/binding structure. Expected objective: -2.
- `test_multiple_opt.lp`: 2 variables, 3 constraints. Has multiple optimal solutions along `x + y = 4`. Expected objective: -4.
- `test_mixed.lp`: 3 variables, 3 constraints. Uses mixed positive and negative objective/constraint coefficients. Expected objective: -19.3333333333.
- `test_larger.lp`: 5 variables, 5 constraints. A larger small-scale regression case. Expected objective: -85.

The models intentionally use only LP syntax currently accepted by the Phase 2 parser: minimization, nonnegative variables by default, and `<=` constraints.

## Running

From the repository root:

```bat
python run_benchmarks.py
```

The benchmark runner automatically searches for HiGHS using `HIGHS_EXE`, the system PATH, and common sibling build locations.

Use `--solver` or `--highs` to override executable locations, and `--tolerance` to change the objective comparison tolerance.

## Netlib validation subset

The repository also contains a curated manifest at `benchmarks/netlib/manifest.txt`. The model files are intentionally not committed; the harness downloads them from the Netlib LP data repository when requested. Netlib is the source repository for these public mathematical optimization data files. urlNetlib LP data repositoryhttps://www.netlib.org/lp/data/

From the repository root, download the subset and compare it against HiGHS:

```bat
python run_netlib_benchmarks.py --download
```

To run only models that have already been downloaded:

```bat
python run_netlib_benchmarks.py
```

To run selected models from the manifest:

```bat
python run_netlib_benchmarks.py --download afiro adlittle blend
```

The harness requires the Phase 2 CLI to report `OPTIMAL` with a passing certificate and requires its objective to agree with HiGHS within the configured tolerance. A model that the current Phase 2 implementation cannot parse or solve is reported as a failure rather than silently omitted.
