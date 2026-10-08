# Phase 6.11 Architecture Visualization

The architecture screen documents the implemented solver control path from the Phase 6 web control center into the C++ solver and Phase 3–5 optimization layers.

Layers represented:
- Phase 6 React/Vite control center
- Phase 6.5 typed solver API boundary
- Phase 5.11 ProductionSolver
- Phase 5 unified GPU solver pipeline
- Phase 3 sparse pricing
- Phase 4 basis solver, FTRAN and BTRAN
- Persistent workspace
- Adaptive CPU/CUDA backend selection
- Numerical stability and CPU fallback
- Benchmarking and verification

The visualization does not claim that CUDA is executing on the current machine. Actual runtime GPU state remains authoritative from the Phase 6.8 runtime endpoint.

Node status meanings:
- runtime: web/runtime control surface
- implemented: architecture exists in the solver project
- planned: reserved future architecture
