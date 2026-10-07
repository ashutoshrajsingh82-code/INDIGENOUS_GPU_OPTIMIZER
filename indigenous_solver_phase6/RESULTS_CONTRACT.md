# Phase 6.7 — Results + Solution Explorer

The frontend consumes authoritative completed-solve data from:

`GET /solve/{jobId}/result`

The response includes objective, iterations, elapsed time, backend, GPU activity, variables, constraints, and numerical certificate data.

No result is generated locally. If the API is unavailable or does not provide a completed result, the UI reports that state.

## Expected result contract

`jobId`, `modelId`, `status`, `objective`, `iterations`, `elapsedMs`, `variables`, `constraints`, `certificate`, `backend`, `gpuActive`, and `message`.
