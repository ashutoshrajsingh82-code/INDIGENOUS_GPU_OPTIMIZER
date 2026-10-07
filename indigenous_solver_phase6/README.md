# Indigenous Solver Phase 6

Phase 6 is the web application layer for the Indigenous GPU Optimizer.

## Roadmap

6.1 React/Vite foundation  
6.2 Dashboard  
6.3 Upload LP/MPS  
6.4 Solver configuration  
6.5 API integration  
6.6 Live solver screen  
6.7 Results + solution explorer  
6.8 GPU monitoring  
6.9 Benchmark dashboard  
6.10 Verification dashboard  
6.11 Architecture visualization  
6.12 Export/reporting

## 6.1 React/Vite foundation

The foundation provides:

- React + TypeScript application shell
- Vite development/build tooling
- typed solver runtime models
- solver-oriented navigation placeholders
- explicit CPU/CUDA runtime state
- separate API boundary for later backend integration
- responsive desktop/tablet/mobile layout
- no false GPU activation on CPU-only hosts

## Run

From the repository root:

    cd indigenous_solver_phase6\frontend
    npm install
    npm run typecheck
    npm run build
    npm run dev

Open the local Vite URL shown in the terminal, normally http://127.0.0.1:5173.

Phase 6.1 does not require a solver API. API integration is intentionally deferred to 6.5.
