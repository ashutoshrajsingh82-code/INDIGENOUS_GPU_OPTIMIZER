# Indigenous Optimization Solver — Phase 1 Foundation

A from-scratch C++17 research foundation for an LP/MILP/QP solver. Phase 1 provides sparse CSC/CSR matrices, basic LP/MPS readers, LU factorization, a restricted canonical primal simplex, a dual-simplex interface reserved for Phase 2, CLI inspection/solve commands, and tests.

## Build

```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

On Windows CMD with Visual Studio, the executable is normally `build\\Release\\solver_cli.exe`.

## CLI

```bash
solver_cli inspect examples/lp/simple.lp
solver_cli solve examples/lp/simple.lp --method primal
solver_cli solve examples/lp/simple.mps --method primal
```

Phase 1 intentionally does not claim production numerical robustness, advanced dual simplex, MILP, QP, or GPU acceleration. Those are later phases.
