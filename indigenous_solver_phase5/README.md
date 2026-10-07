# Phase 5 — Unified GPU Solver
## Phase 5.1: Unified GPU Solver Pipeline
Phase 5.1 introduces a thin orchestration layer coordinating the validated Phase 3 sparse-pricing workspace and Phase 4 basis backend for FTRAN, BTRAN, and basis updates.
It intentionally does not duplicate pricing or basis linear algebra. On the Intel-only development machine, the pipeline executes through CPU basis and CPU pricing fallbacks.

## Phase 5.2: Persistent GPU / Solver Workspace
Phase 5.2 adds a unified persistent operation workspace for the Phase 5 pipeline.

### Ownership model
- Phase 4.8 CudaBasisWorkspace remains the owner of CUDA-resident basis matrices, permutation data, SpSV descriptors, analysis state, and basis solve buffers.
- Phase 3 SparsePricingWorkspace remains the owner of immutable CSC/objective pricing data and its CUDA-resident representation when CUDA is available.
- Phase 5.2 UnifiedGpuWorkspace owns reusable cross-operation solver buffers for FTRAN, BTRAN, pricing output, and pivot/update data.

This prevents Phase 5 from creating a second copy of the Phase 4 basis device state.

### Persistent behavior
A buffer is allocated or grown when its capacity is insufficient. Subsequent requests that fit the existing capacity are counted as reuses. The same logical buffers are therefore retained across repeated pipeline calls.

On the current Intel-only machine these are persistent host buffers. Actual CUDA device-memory execution remains pending an NVIDIA/CUDA machine.

Phase 5.2 still does not modify the Phase 2 RevisedSimplex iteration loop or introduce asynchronous execution; those belong to later Phase 5 stages.
