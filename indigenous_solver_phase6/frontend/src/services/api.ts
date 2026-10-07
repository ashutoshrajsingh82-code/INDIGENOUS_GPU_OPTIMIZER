import type { SolverConfiguration } from "../types/solverConfig";
import type { ModelSummary } from "../types/model";

export interface ApiConfig { baseUrl: string; timeoutMs: number; }
export const apiConfig: ApiConfig = {
  baseUrl: import.meta.env.VITE_SOLVER_API_URL ?? "http://127.0.0.1:8080",
  timeoutMs: 15000,
};

export interface SolverHealth {
  status: "ok" | "degraded" | "unavailable";
  executionBackend: "CPU" | "CUDA" | "UNKNOWN";
  cudaCompiled: boolean;
  cudaDeviceReady: boolean;
  cpuFallbackEnabled: boolean;
  version?: string;
}

export interface ModelInspectionResponse extends ModelSummary {
  modelId: string;
}

export interface SolveRequest {
  modelId: string;
  configuration: SolverConfiguration;
}

export interface SolveResponse {
  jobId: string;
  modelId?: string;
  status: "queued" | "running" | "optimal" | "infeasible" | "unbounded" | "iteration_limit" | "error";
  message: string;
  progress?: number;
  iteration?: number;
  objective?: number | null;
  elapsedMs?: number;
}

async function request<T>(path: string, init?: RequestInit): Promise<T> {
  const controller = new AbortController();
  const timer = window.setTimeout(() => controller.abort(), apiConfig.timeoutMs);
  try {
    const response = await fetch(apiConfig.baseUrl + path, {
      ...init,
      signal: controller.signal,
    });
    const text = await response.text();
    let body: unknown = undefined;
    if (text) {
      try { body = JSON.parse(text); } catch { body = { message: text }; }
    }
    if (!response.ok) {
      const message = typeof body === "object" && body && "message" in body ? String((body as { message: unknown }).message) : "Solver API request failed.";
      throw new Error(message);
    }
    return body as T;
  } finally {
    window.clearTimeout(timer);
  }
}

export function apiHealth(): Promise<SolverHealth> {
  return request<SolverHealth>("/health");
}

export function inspectModel(file: File): Promise<ModelInspectionResponse> {
  const form = new FormData();
  form.append("model", file);
  return request<ModelInspectionResponse>("/models/inspect", { method: "POST", body: form });
}

export function solveModel(requestBody: SolveRequest): Promise<SolveResponse> {
  return request<SolveResponse>("/solve", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(requestBody),
  });
}

export function getSolveStatus(jobId: string): Promise<SolveResponse> {
  return request<SolveResponse>("/solve/" + encodeURIComponent(jobId));
}

export interface SolveResultResponse {
  jobId: string;
  modelId: string;
  status: "optimal" | "infeasible" | "unbounded" | "iteration_limit" | "error";
  objective: number | null;
  iterations: number;
  elapsedMs: number;
  variables: Array<{ name: string; value: number; reducedCost?: number | null; lowerBound?: number | null; upperBound?: number | null }>;
  constraints: Array<{ name: string; activity: number; rhs: number; dualValue?: number | null; residual?: number | null }>;
  certificate: { passed: boolean; primalResidual?: number; dualResidual?: number; complementarityResidual?: number; message: string };
  backend: "CPU" | "CUDA" | "UNKNOWN";
  gpuActive: boolean;
  message: string;
}

export function getSolveResult(jobId: string): Promise<SolveResultResponse> {
  return request<SolveResultResponse>("/solve/" + encodeURIComponent(jobId) + "/result");
}
