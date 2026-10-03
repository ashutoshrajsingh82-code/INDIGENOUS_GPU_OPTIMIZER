# Phase 1 — Foundation

Implemented foundation: sparse CSC/CSR representation and conversion, LP/MPS readers, model validation, basic LU with partial pivoting, restricted canonical primal simplex, dual-simplex interface, CLI, examples, and automated tests.

## Current simplex boundary
The initial primal simplex accepts nonnegative variables and `<=` constraints with nonnegative right-hand sides (plus finite upper bounds converted to rows). Equality, `>=`, negative-RHS feasibility restoration, and advanced basis management are reserved for Phase 2.

## Next phase
Phase 2 adds robust dual simplex, sparse factorization/update machinery, scaling, presolve/postsolve, perturbation, stronger ratio tests, and numerical refinement.
