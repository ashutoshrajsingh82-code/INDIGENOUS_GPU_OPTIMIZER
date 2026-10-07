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

## Phase 5.4: Adaptive CPU/GPU Workload Selection
Phase 5.4 adds an explicit workload-aware backend policy above the Phase 5.1-5.3 orchestration layer.

### Selection policy
The adaptive selector considers:
- whether a CUDA-capable device is actually available at runtime,
- whether GPU preference is enabled,
- problem dimension,
- sparse nonzero count,
- an estimated operation workload.

Small or sparse workloads stay on CPU to avoid GPU launch, transfer, and synchronization overhead. Larger workloads are eligible for GPU execution when CUDA is genuinely available.

Default thresholds are:
- minimum dimension: 256,
- minimum nonzeros: 4096,
- minimum estimated work: 1,000,000.

The thresholds are policy values, not correctness requirements. They can be changed through AdaptiveBackendSelector::Options.

### Reporting
The pipeline now reports both:
- the backend that is actually executing, and
- the adaptive backend recommendation.

This distinction is intentional. Phase 5.4 does not falsely claim GPU execution on a machine without an NVIDIA/CUDA device.

On the current Intel-only machine:
- CUDA availability is false,
- basis adaptive recommendation is CPU,
- pricing adaptive recommendation is CPU,
- aggregate adaptive GPU eligibility is false.

On an NVIDIA/CUDA machine, large workloads can be recommended for GPU while small workloads remain on CPU.

Phase 5.4 remains a backend-selection policy layer; later phases can use these decisions for asynchronous execution, batching, and production routing.


## Phase 5.5: Asynchronous GPU / Pipeline Execution
Phase 5.5 adds a backend-neutral asynchronous execution boundary around the
unified solver pipeline.

### Execution model
- AsyncExecutionEngine submits operations with an explicit asynchronous
  std::launch::async policy and tracks submitted, completed, and in-flight
  tasks.
- UnifiedGpuSolverPipeline::coordinate_iteration_async() dispatches the
  complete BTRAN -> pricing -> FTRAN sequence without blocking the caller.
- The async pipeline serializes access to the shared Phase 5.2 persistent
  operation workspace, preventing concurrent tasks from racing reusable
  buffers.
- Results are returned through shared asynchronous result state and are
  validated after wait_async().

### Backend reporting
Asynchronous dispatch is reported separately from GPU activity:
- CPU-ASYNC means the asynchronous task boundary is active while the
  underlying pipeline is using the CPU backend.
- GPU execution is reported as active only when the Phase 3/4 CUDA backends
  actually report GPU activity.
- async_gpu_capable therefore remains false on the current Intel-only
  machine.

This distinction prevents asynchronous CPU execution from being incorrectly
reported as CUDA execution.

Phase 5.5 is an execution-orchestration stage. It does not claim direct CUDA
stream control for Phase 3/4 kernels whose existing APIs are synchronous.
Those lower-level stream/batched operation changes belong to later GPU
execution stages.


## Phase 5.6: Batch / Multi-Vector GPU Operations
Phase 5.6 adds a backend-neutral batch API for processing multiple independent
FTRAN, BTRAN, pricing, or complete BTRAN -> pricing -> FTRAN operations
through the same Phase 5 pipeline.

### Batch API
The pipeline exposes:
- `batch_ftran()` for multiple FTRAN right-hand sides,
- `batch_btran()` for multiple BTRAN right-hand sides,
- `batch_price()` for multiple dual vectors,
- `batch_coordinate_iteration()` for multiple complete simplex
  linear-algebra iterations.

Each batch is validated before execution so vector dimensions remain
consistent with the initialized basis.

### Workspace and backend behavior
The batch API reuses the existing Phase 5.2 persistent workspace and the
validated Phase 3/4 backends. On the current Intel-only machine, batch
operations execute through the CPU backend as a logical batch. The current
CPU implementation intentionally processes vectors sequentially through the
persistent buffers; it does not claim SIMD or CUDA kernel batching.

This establishes the stable multi-vector interface for a CUDA implementation:
an NVIDIA/CUDA backend can replace the per-vector loop with batched
cuSPARSE/device-kernel dispatch while preserving the public pipeline API and
ownership model.

### Reporting
Phase 5.6 reports:
- number of batch calls,
- number of vectors processed,
- batch API availability,
- actual batch backend.

On the current Intel machine the expected backend is `CPU-BATCH`. A future
CUDA implementation should report `CUDA-BATCH` only when CUDA execution is
actually active.

The Phase 5.5 asynchronous boundary remains separate. Batch operations are
serialized against the shared persistent workspace so they cannot race an
in-flight asynchronous operation.
