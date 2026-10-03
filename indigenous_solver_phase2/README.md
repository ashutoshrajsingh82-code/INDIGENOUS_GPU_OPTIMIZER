# Phase 2 — LP Core

Phase 2 adds the LP-core architecture described in the project design:

- sparse basis factorization with pivot tolerance and transpose solves;
- revised simplex interfaces with FTRAN/BTRAN pricing;
- Harris-style ratio selection;
- deterministic refactorization after pivots;
- scaling and presolve modules as explicit solver stages;
- independent primal/objective certificate through the existing validator;
- a dedicated Phase 2 CLI and CTest target.

## Current numerical gate

The first Phase 2 solver gate intentionally accepts the canonical subset with finite variable lower bounds and <= constraints. Equality and >= constraints return UNSUPPORTED_MODEL rather than being silently transformed incorrectly. This is deliberate: the next gate should add a tested artificial-variable Phase I and postsolve mapping.

The basis update path currently refactorizes the complete basis after each pivot. This prioritizes correctness and reproducibility; Forrest–Tomlin/product-form updates are a later performance optimization.

## Build

    cmake -S indigenous_solver_phase2 -B build
    cmake --build build
    ctest --test-dir build --output-on-failure

Run:

    ./build/solver_phase2_cli solve indigenous_solver_phase1/examples/lp/simple.lp

Phase 2 does not yet claim full Netlib coverage. The milestone is a correct revised-simplex foundation that can be expanded without replacing the solver core.