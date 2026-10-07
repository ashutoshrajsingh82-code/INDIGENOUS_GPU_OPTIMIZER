import type { SolverConfiguration } from "./solverConfig";

export type SolveJobStatus =
  | "idle" | "submitting" | "queued" | "running"
  | "optimal" | "infeasible" | "unbounded" | "iteration_limit" | "error";

export interface SolveJob {
  jobId: string | null;
  modelId: string | null;
  status: SolveJobStatus;
  message: string;
  startedAt: string | null;
  updatedAt: string | null;
  configuration: SolverConfiguration | null;
}

export interface VariableSolution {
  name: string;
  value: number;
  reducedCost?: number | null;
  lowerBound?: number | null;
  upperBound?: number | null;
}

export interface ConstraintSolution {
  name: string;
  activity: number;
  rhs: number;
  dualValue?: number | null;
  residual?: number | null;
}

export interface SolverCertificate {
  passed: boolean;
  primalResidual?: number;
  dualResidual?: number;
  complementarityResidual?: number;
  message: string;
}

export interface SolveResult {
  jobId: string;
  modelId: string;
  status: "optimal" | "infeasible" | "unbounded" | "iteration_limit" | "error";
  objective: number | null;
  iterations: number;
  elapsedMs: number;
  variables: VariableSolution[];
  constraints: ConstraintSolution[];
  certificate: SolverCertificate;
  backend: "CPU" | "CUDA" | "UNKNOWN";
  gpuActive: boolean;
  message: string;
}
