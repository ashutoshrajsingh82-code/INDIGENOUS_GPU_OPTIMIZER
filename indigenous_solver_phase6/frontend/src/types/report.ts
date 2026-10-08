import type { BenchmarkSnapshot } from "./benchmark";
import type { VerificationSnapshot } from "./verification";
import type { RuntimeSnapshot } from "./runtime";
import type { ArchitectureSnapshot } from "./architecture";
import type { SolveResultResponse } from "../services/api";

export interface ReportModel {
  name?: string;
  modelId?: string;
  format?: string;
  rows?: number | null;
  columns?: number | null;
  nonzeros?: number | null;
}
export interface SolverReport {
  reportId: string;
  generatedAt: string;
  jobId?: string | null;
  model: ReportModel;
  result?: SolveResultResponse | null;
  runtime?: RuntimeSnapshot | null;
  benchmark?: BenchmarkSnapshot | null;
  verification?: VerificationSnapshot | null;
  architecture?: ArchitectureSnapshot | null;
  message?: string;
}
export interface ReportResponse {
  status: "ok" | "unavailable";
  report: SolverReport | null;
  message?: string;
}