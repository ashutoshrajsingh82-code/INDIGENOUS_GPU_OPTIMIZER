export type RuntimeExecutionBackend = "CPU" | "CUDA" | "UNKNOWN";

export interface RuntimeSnapshot {
  status: "ok" | "degraded" | "unavailable";
  executionBackend: RuntimeExecutionBackend;
  cudaCompiled: boolean;
  cudaDeviceReady: boolean;
  gpuRuntimeActive: boolean;
  basisGpuActive: boolean;
  pricingGpuActive: boolean;
  ftranCalls: number;
  btranCalls: number;
  pricingCalls: number;
  updateCalls: number;
  workspaceAllocations: number;
  workspaceReuses: number;
  workspacePersistent: boolean;
  asyncBackend: string;
  batchBackend: string;
  adaptiveGpuEligible: boolean;
  numericalStable: boolean;
  numericalChecks: number;
  numericalFailures: number;
  fallbackCount: number;
  bottleneck: string;
  bottleneckShare: number;
  version?: string;
  message?: string;
}
