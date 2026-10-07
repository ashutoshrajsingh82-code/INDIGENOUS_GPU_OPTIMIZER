# Phase 6.5 — API Integration

This phase establishes the browser-to-solver API boundary.

## API contract

Default development server:

`http://127.0.0.1:8080`

Endpoints:

- `GET /health` — runtime/backend health.
- `POST /models/inspect` — multipart field `model`; returns authoritative LP/MPS dimensions and a `modelId`.
- `POST /solve` — accepts `{ modelId, configuration }`; returns a `jobId`.
- `GET /solve/{jobId}` — returns solve status.

The React application does not execute the optimization algorithm. The authoritative implementation remains the C++ ProductionSolver/Phase 5 pipeline.

## Configuration handoff

The Phase 6.4 configuration is serialized without transformation:

- revised-simplex method
- backend policy
- maximum iterations
- relative/absolute/pivot tolerances
- CPU fallback
- numerical validation
- profiling

## Development

Set `VITE_SOLVER_API_URL` when the API is not on port 8080.

Example:

`VITE_SOLVER_API_URL=http://127.0.0.1:8080`

If the API is not running, the frontend must report the connection failure rather than fabricate solver results.
