export type ApiSolveStatus = "queued" | "running" | "optimal" | "infeasible" | "unbounded" | "iteration_limit" | "error";

export interface SolverHealth {
  status: "ok" | "degraded" | "unavailable";
  executionBackend: "CPU" | "CUDA" | "UNKNOWN";
  cudaCompiled: boolean;
  cudaDeviceReady: boolean;
  cpuFallbackEnabled: boolean;
  version?: string;
}
