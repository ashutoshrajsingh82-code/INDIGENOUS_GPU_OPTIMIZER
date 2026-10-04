# LP Benchmarks

This directory contains LP instances used to compare the indigenous Phase 2 revised-simplex solver against HiGHS.

## Included models

- `test_small.lp`: 2 variables, 3 constraints. Expected objective: -6.
- `test_medium.lp`: 3 variables, 3 constraints. Expected objective: -34.
- `test_bounds.lp`: 2 variables, 3 constraints. Exercises several binding constraints. Expected objective: -22.
- `test_degenerate.lp`: 2 variables, 4 constraints. Contains redundant/binding structure. Expected objective: -2.
- `test_multiple_opt.lp`: 2 variables, 3 constraints. Has multiple optimal solutions along `x + y = 4`. Expected objective: -4.
- `test_mixed.lp`: 3 variables, 3 constraints. Uses mixed positive and negative objective/constraint coefficients. Expected objective: -24.
- `test_larger.lp`: 5 variables, 5 constraints. A larger small-scale regression case. Expected objective: -95.

The models intentionally use only LP syntax currently accepted by the Phase 2 parser: minimization, nonnegative variables by default, and `<=` constraints.

## Running

From the repository root:

```bat
python run_benchmarks.py
```

The benchmark runner automatically searches for HiGHS using `HIGHS_EXE`, the system PATH, and common sibling build locations.

Use `--solver` or `--highs` to override executable locations, and `--tolerance` to change the objective comparison tolerance.
