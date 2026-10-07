import type { SolverConfiguration } from "../types/solverConfig";

export interface ConfigurationValidation {
  valid: boolean;
  errors: Partial<Record<keyof SolverConfiguration, string>>;
}

export function validateSolverConfiguration(config: SolverConfiguration): ConfigurationValidation {
  const errors: ConfigurationValidation["errors"] = {};
  if (!Number.isInteger(config.maxIterations) || config.maxIterations < 1 || config.maxIterations > 10_000_000) {
    errors.maxIterations = "Maximum iterations must be an integer from 1 to 10,000,000.";
  }
  for (const [key, value, label] of [
    ["relativeTolerance", config.relativeTolerance, "Relative tolerance"],
    ["absoluteTolerance", config.absoluteTolerance, "Absolute tolerance"],
    ["pivotTolerance", config.pivotTolerance, "Pivot tolerance"],
  ] as const) {
    if (!Number.isFinite(value) || value <= 0 || value > 1) errors[key] = label + " must be greater than 0 and at most 1.";
  }
  if (config.backendPolicy === "cuda" && !config.enableCpuFallback) {
    errors.enableCpuFallback = "CUDA-only mode disables the safety fallback. Enable CPU fallback unless running on a verified CUDA host.";
  }
  return { valid: Object.keys(errors).length === 0, errors };
}
