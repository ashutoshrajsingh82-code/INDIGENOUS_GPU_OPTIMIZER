# LP Benchmarks

This directory contains small LP instances used to compare the indigenous Phase 2 revised-simplex solver against HiGHS.

## Included models

- `test_small.lp`: 2 variables, 3 constraints. Expected objective: -6.
- `test_medium.lp`: 3 variables, 3 constraints. Expected objective: -34.

The models intentionally use the LP syntax currently accepted by the Phase 2 parser.

## Running

From the repository root:

```bat
python run_benchmarks.py --highs C:\path\to\highs.exe
```

On Windows, the script also checks the default sibling HiGHS build location:

```
..\HiGHS\build\Release\bin\highs.exe
```

Use `--solver` to override the indigenous solver executable and `--tolerance` to change the objective comparison tolerance.
