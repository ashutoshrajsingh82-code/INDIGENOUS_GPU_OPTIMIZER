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


## Phase 5.3: GPU Pricing + FTRAN/BTRAN Coordination
Phase 5.3 coordinates the three linear-algebra operations used by a revised-simplex iteration without duplicating ownership of Phase 3 or Phase 4 state.

### Coordinated sequence
- **BTRAN:** solve (B^T y = b) using the Phase 4 basis backend.
- **Pricing:** compute (r = c - A^T y) using the Phase 3 sparse-pricing workspace.
- **FTRAN:** solve (B d = a_{enter}) using the Phase 4 basis backend.

The pipeline exposes:
- `btran_and_price()` for dual construction followed by reduced-cost pricing.
- `price_and_ftran()` for reduced-cost pricing followed by entering-column FTRAN.
- `coordinate_iteration()` for the complete BTRAN -> pricing -> FTRAN sequence.

The same Phase 5.2 persistent operation workspace is reused across these calls. Phase 4.8 remains the owner of CUDA-resident basis state, and Phase 3 remains the owner of CUDA-resident immutable CSC pricing state.

### Mixed CPU/GPU reporting
Phase 5.3 separates:
- basis GPU activity,
- pricing GPU activity,
- aggregate GPU activity.

This avoids incorrectly reporting the entire pipeline as CPU when pricing is on CUDA but the basis backend is still CPU.

Phase 5.3 still does not replace the Phase 2 RevisedSimplex iteration loop. It provides the backend-neutral coordinated linear-algebra path that a later production integration stage can call.
