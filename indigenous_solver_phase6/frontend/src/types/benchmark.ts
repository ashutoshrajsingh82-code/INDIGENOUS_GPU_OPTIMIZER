export type BenchmarkBackend = "CPU" | "CUDA" | "UNKNOWN";
export interface BenchmarkCase {
  model: string; phase2Ms: number | null; phase3Ms: number | null; phase4Ms: number | null;
  speedup: number | null; backend: BenchmarkBackend;
  status: "optimal" | "infeasible" | "unbounded" | "iteration_limit" | "error" | "unavailable";
  certificatePassed: boolean; iterations: number | null; objective: number | null; message?: string;
}
export interface BenchmarkSummary {
  status: "ok" | "unavailable"; modelCount: number;
  phase2TotalMs: number | null; phase3TotalMs: number | null; phase4TotalMs: number | null;
  aggregateSpeedup: number | null; phase3FasterCount: number; phase4FasterCount: number;
  certificatePassCount: number; gpuRuns: number; cpuRuns: number; timestamp?: string; message?: string;
}
export interface BenchmarkSnapshot { summary: BenchmarkSummary; cases: BenchmarkCase[]; }
