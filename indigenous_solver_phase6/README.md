# Phase 6 Native Solver API

The Phase 6 API is a thin native HTTP boundary around the existing Phase 5 `ProductionSolver`.

## Build

From a Visual Studio 2022 x64 developer command prompt:

```cmd
cd C:\Users\ASHUTOSH\INDIGENOUS_GPU_OPTIMIZER\INDIGENOUS_GPU_OPTIMIZER
cmake -S indigenous_solver_phase6 -B indigenous_solver_phase6\build -G "NMake Makefiles"
cmake --build indigenous_solver_phase6\build
```

Run:

```cmd
indigenous_solver_phase6\build\indigenous_phase6_api_server.exe --port 8080
```

The server binds only to `127.0.0.1`.

## Endpoints

- `GET /health`
- `POST /models/inspect` — multipart field `model`
- `POST /solve` — JSON `{ modelId, configuration }`
- `GET /solve/{jobId}`
- `GET /solve/{jobId}/result`
- `GET /runtime`
- `GET /verification`
- `GET /benchmarks`
- `GET /reports/latest`
- `GET /reports/{jobId}`

The API stores inspected models and completed jobs in process memory. Restarting the server clears them.

## GPU behavior

The API does not claim CUDA execution when the machine has no CUDA runtime/device. On the current Intel-only development machine, the authoritative backend remains CPU. A request with `backendPolicy=cuda` is rejected instead of silently falling back.

The existing Phase 3/4/5 CUDA auto-detection remains authoritative when this project is built on an NVIDIA/CUDA machine.
