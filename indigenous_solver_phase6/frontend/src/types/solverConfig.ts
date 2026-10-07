export type BackendPolicy = "auto" | "cpu" | "cuda";
export type SolveMethod = "revised-simplex";

export interface SolverConfiguration {
  method: SolveMethod;
  backendPolicy: BackendPolicy;
  maxIterations: number;
  relativeTolerance: number;
  absoluteTolerance: number;
  pivotTolerance: number;
  enableCpuFallback: boolean;
  enableNumericalValidation: boolean;
  enableProfiling: boolean;
}

export const defaultSolverConfiguration: SolverConfiguration = {
  method: "revised-simplex",
  backendPolicy: "auto",
  maxIterations: 10000,
  relativeTolerance: 1e-9,
  absoluteTolerance: 1e-12,
  pivotTolerance: 1e-12,
  enableCpuFallback: true,
  enableNumericalValidation: true,
  enableProfiling: true,
};
