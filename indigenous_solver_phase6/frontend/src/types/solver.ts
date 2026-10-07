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
  icon: string;
}

export const runtimeStatus: RuntimeStatus = {
  backend: "CPU",
  cudaCompiled: false,
  deviceReady: false,
  gpuActive: false,
  cpuFallback: true,
};

export const navigation: SolverNavigationItem[] = [
  { id: "dashboard", label: "Dashboard", phase: "6.2", icon: "⌂" },
  { id: "models", label: "Models", phase: "6.3", icon: "▣" },
  { id: "solve", label: "Solve", phase: "6.4", icon: "▶" },
  { id: "live", label: "Live Solver", phase: "6.6", icon: "◉" },
  { id: "results", label: "Results", phase: "6.7", icon: "◈" },
  { id: "gpu", label: "GPU Monitor", phase: "6.8", icon: "▥" },
  { id: "benchmarks", label: "Benchmarks", phase: "6.9", icon: "↗" },
  { id: "verification", label: "Verification", phase: "6.10", icon: "✓" },
  { id: "architecture", label: "Architecture", phase: "6.11", icon: "◇" },
  { id: "reports", label: "Reports", phase: "6.12", icon: "▤" },
];
