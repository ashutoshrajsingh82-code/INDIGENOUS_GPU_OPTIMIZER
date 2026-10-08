# Phase 6 Native API Contract

The C++ API owns model parsing, optimization, certificates, runtime state, and report generation. React only calls these endpoints.

## Endpoints

- `GET /health`
- `POST /models/inspect`
- `POST /solve`
- `GET /solve/{jobId}`
- `GET /solve/{jobId}/result`
- `GET /runtime`
- `GET /verification`
- `GET /benchmarks`
- `GET /reports/latest`
- `GET /reports/{jobId}`

## Report ownership

`GET /reports/{jobId}` returns the authoritative completed-job snapshot. It contains the Phase 5 ProductionSolver result, certificate, runtime state, verification summary, and model metadata.

Benchmark data is returned as unavailable until the benchmark service is connected to the API. It is never fabricated.

## Runtime model

Jobs are solved synchronously in the native API implementation, but the frontend polling contract remains compatible. Inspected models and completed jobs are process-local; restarting the server clears them.

The native server binds to localhost and is intended as the Phase 6 local development boundary.
