# Phase 6.8 Runtime Monitoring Contract

The frontend consumes authoritative runtime state from the C++ solver API.

## Endpoint

GET /runtime

## Response fields

status, executionBackend, cudaCompiled, cudaDeviceReady, gpuRuntimeActive, basisGpuActive, pricingGpuActive, ftranCalls, btranCalls, pricingCalls, updateCalls, workspaceAllocations, workspaceReuses, workspacePersistent, asyncBackend, batchBackend, adaptiveGpuEligible, numericalStable, numericalChecks, numericalFailures, fallbackCount, bottleneck, bottleneckShare, version, message.

## Semantics

- cudaCompiled means the backend was compiled with CUDA support.
- cudaDeviceReady means the C++ runtime detected a usable CUDA device.
- gpuRuntimeActive means a solver operation actually executed through the GPU backend.
- basisGpuActive and pricingGpuActive identify active GPU subsystems.
- Operation counters are authoritative runtime counters, not browser estimates.
- adaptiveGpuEligible is the Phase 5 adaptive selector decision for the current runtime/workload.
- fallbackCount counts explicit runtime fallback events.
- The frontend must not infer CUDA state from browser WebGPU, WebGL, or browser hardware.

If /runtime is unavailable, the UI displays an explicit unavailable state rather than fabricating CPU/GPU metrics.
