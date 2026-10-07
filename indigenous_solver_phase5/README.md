# Phase 5 — Unified GPU Solver
## Phase 5.1: Unified GPU Solver Pipeline
Phase 5.1 introduces a thin orchestration layer coordinating the validated Phase 3 sparse-pricing workspace and Phase 4 basis backend for FTRAN, BTRAN, and basis updates.
It intentionally does not duplicate pricing or basis linear algebra.
On the current Intel-only development machine, the pipeline executes through CPU basis and CPU pricing fallbacks.
Phase 5.1 does not yet modify the Phase 2 RevisedSimplex implementation, introduce asynchronous execution, or move the entire simplex state to GPU. Those responsibilities belong to later Phase 5 sub-phases.
