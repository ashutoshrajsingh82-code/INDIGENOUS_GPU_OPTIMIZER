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
export const terminalStatuses: SolveJobStatus[] = ["optimal","infeasible","unbounded","iteration_limit","error"];
export function isTerminalStatus(status: SolveJobStatus): boolean {
  return terminalStatuses.includes(status);
}
