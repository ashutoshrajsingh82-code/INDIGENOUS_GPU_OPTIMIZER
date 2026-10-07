export type ExecutionBackend = "CPU" | "CUDA";

export interface RuntimeStatus {
  backend: ExecutionBackend;
  cudaCompiled: boolean;
  deviceReady: boolean;
  gpuActive: boolean;
  cpuFallback: boolean;
}

export interface SolverNavigationItem {
  id: string;
  label: string;
  phase: string;
}

export const runtimeStatus: RuntimeStatus = {
  backend: "CPU",
  cudaCompiled: false,
  deviceReady: false,
  gpuActive: false,
  cpuFallback: true,
};

export const navigation: SolverNavigationItem[] = [
  { id: "dashboard", label: "Dashboard", phase: "6.2" },
  { id: "models", label: "Models", phase: "6.3" },
  { id: "solve", label: "Solve", phase: "6.4" },
  { id: "results", label: "Results", phase: "6.7" },
  { id: "gpu", label: "GPU Monitor", phase: "6.8" },
  { id: "benchmarks", label: "Benchmarks", phase: "6.9" },
  { id: "verification", label: "Verification", phase: "6.10" },
  { id: "architecture", label: "Architecture", phase: "6.11" },
  { id: "reports", label: "Reports", phase: "6.12" },
];
