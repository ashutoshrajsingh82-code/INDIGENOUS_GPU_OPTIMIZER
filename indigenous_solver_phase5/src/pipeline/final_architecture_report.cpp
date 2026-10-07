#include "pipeline/final_architecture_report.hpp"

#include <iomanip>
#include <sstream>

namespace indigenous::pipeline {

FinalArchitectureReport::Snapshot FinalArchitectureReport::build(
    const UnifiedGpuSolverPipeline::Report& pipeline,
    const ProductionSolver::Report& production) {
  Snapshot s;
  s.execution_backend = pipeline.gpu_active ? "CUDA" : "CPU";
  s.basis_backend = pipeline.basis_backend ? pipeline.basis_backend : "NONE";
  s.pricing_backend =
      pipeline.pricing_backend ? pipeline.pricing_backend : "UNKNOWN";
  s.adaptive_basis_backend =
      pipeline.adaptive_basis_backend ? pipeline.adaptive_basis_backend : "CPU";
  s.adaptive_pricing_backend =
      pipeline.adaptive_pricing_backend ? pipeline.adaptive_pricing_backend : "CPU";
  s.async_backend = pipeline.async_backend ? pipeline.async_backend : "CPU-ASYNC";
  s.batch_backend = pipeline.batch_backend ? pipeline.batch_backend : "CPU-BATCH";
  s.sparse_strategy =
      pipeline.sparse_strategy ? pipeline.sparse_strategy : "direct sparse CPU";
  s.sparse_scale = pipeline.sparse_scale ? pipeline.sparse_scale : "SMALL";
  s.bottleneck =
      pipeline.performance.bottleneck ? pipeline.performance.bottleneck : "NONE";
  s.status = production.solved
                 ? (production.optimal ? "OPTIMAL" : "SOLVED_NONOPTIMAL")
                 : "NOT_SOLVED";
  s.message = production.message;

  s.production_solved = production.solved;
  s.production_optimal = production.optimal;
  s.pipeline_initialized = pipeline.initialized;
  s.pipeline_preflight = production.pipeline_preflight;
  s.pipeline_preflight_passed = production.pipeline_preflight_passed;

  s.gpu_active = pipeline.gpu_active;
  s.basis_gpu_active = pipeline.basis_gpu_active;
  s.pricing_gpu_active = pipeline.pricing_gpu_active;
  s.adaptive_gpu_eligible = pipeline.adaptive_gpu_eligible;
  s.basis_gpu_recommended = pipeline.basis_gpu_recommended;
  s.pricing_gpu_recommended = pipeline.pricing_gpu_recommended;
  s.async_gpu_capable = pipeline.async_gpu_capable;
  s.workspace_persistent = pipeline.workspace_persistent;
  s.numerical_stable = pipeline.numerical_stable;
  s.fallback_active = pipeline.fallback_active;
  s.sparse_large_scale = pipeline.sparse_large_scale;

  s.iterations = production.iterations;
  s.ftran_calls = pipeline.ftran_calls;
  s.btran_calls = pipeline.btran_calls;
  s.pricing_calls = pipeline.pricing_calls;
  s.coordination_calls = pipeline.coordination_calls;
  s.updates = pipeline.update_count;
  s.workspace_allocations = pipeline.workspace_allocations;
  s.workspace_reuses = pipeline.workspace_reuses;
  s.adaptive_decisions = pipeline.adaptive_decisions;
  s.async_submitted = pipeline.async_submitted;
  s.async_completed = pipeline.async_completed;
  s.batch_calls = pipeline.batch_calls;
  s.batch_vectors_processed = pipeline.batch_vectors_processed;
  s.sparse_rows = pipeline.sparse_rows;
  s.sparse_columns = pipeline.sparse_columns;
  s.sparse_nonzeros = pipeline.sparse_nonzeros;
  s.recommended_batch_vectors = pipeline.recommended_batch_vectors;
  s.chunk_columns = pipeline.chunk_columns;
  s.numerical_checks = pipeline.numerical_checks;
  s.numerical_failures = pipeline.numerical_failures;
  s.fallback_count = pipeline.fallback_count;

  s.objective = production.objective;
  s.primal_residual = production.primal_residual;
  s.dual_residual = production.dual_residual;
  s.maximum_residual = pipeline.maximum_residual;
  s.bottleneck_percent = pipeline.performance.bottleneck_percent;
  return s;
}

bool FinalArchitectureReport::production_ready(
    const Snapshot& s) noexcept {
  return s.production_solved &&
         s.production_optimal &&
         s.pipeline_preflight &&
         s.pipeline_preflight_passed &&
         s.numerical_stable;
}

bool FinalArchitectureReport::gpu_ready(const Snapshot& s) noexcept {
  return s.gpu_active &&
         (s.basis_gpu_active || s.pricing_gpu_active) &&
         s.async_gpu_capable;
}

std::string FinalArchitectureReport::to_text(const Snapshot& s) {
  std::ostringstream out;
  out << "INDIGENOUS GPU OPTIMIZER - PHASE 5 FINAL ARCHITECTURE REPORT\n";
  out << "==============================================================\n";
  out << "Status: " << s.status << "\n";
  out << "Execution backend: " << s.execution_backend << "\n";
  out << "Basis backend: " << s.basis_backend << "\n";
  out << "Pricing backend: " << s.pricing_backend << "\n";
  out << "Adaptive basis: " << s.adaptive_basis_backend << "\n";
  out << "Adaptive pricing: " << s.adaptive_pricing_backend << "\n";
  out << "Async backend: " << s.async_backend << "\n";
  out << "Batch backend: " << s.batch_backend << "\n";
  out << "GPU active: " << (s.gpu_active ? "YES" : "NO") << "\n";
  out << "GPU runtime ready: " << (gpu_ready(s) ? "YES" : "NO") << "\n";
  out << "Production ready: " << (production_ready(s) ? "YES" : "NO") << "\n";
  out << "Pipeline preflight: "
      << (s.pipeline_preflight_passed ? "PASS" : "FAIL") << "\n";
  out << "Workspace persistent: "
      << (s.workspace_persistent ? "YES" : "NO") << "\n";
  out << "Numerical stable: "
      << (s.numerical_stable ? "YES" : "NO") << "\n";
  out << "Fallback active: "
      << (s.fallback_active ? "YES" : "NO") << "\n";
  out << "Sparse model: " << s.sparse_scale
      << " | NNZ: " << s.sparse_nonzeros
      << " | Strategy: " << s.sparse_strategy << "\n";
  out << "Recommended batch vectors: " << s.recommended_batch_vectors
      << " | Column chunk: " << s.chunk_columns << "\n";
  out << "Iterations: " << s.iterations
      << " | FTRAN: " << s.ftran_calls
      << " | BTRAN: " << s.btran_calls
      << " | Pricing: " << s.pricing_calls
      << " | Coordination: " << s.coordination_calls << "\n";
  out << "Workspace allocations: " << s.workspace_allocations
      << " | Reuses: " << s.workspace_reuses << "\n";
  out << "Async submitted: " << s.async_submitted
      << " | Completed: " << s.async_completed
      << " | Batch calls: " << s.batch_calls
      << " | Batch vectors: " << s.batch_vectors_processed << "\n";
  out << "Numerical checks: " << s.numerical_checks
      << " | Failures: " << s.numerical_failures
      << " | Fallbacks: " << s.fallback_count << "\n";
  out << std::setprecision(12);
  out << "Objective: " << s.objective
      << " | Primal residual: " << s.primal_residual
      << " | Dual residual: " << s.dual_residual << "\n";
  out << "Profiler bottleneck: " << s.bottleneck
      << " | Share: " << s.bottleneck_percent << "%\n";
  if (!s.message.empty())
    out << "Message: " << s.message << "\n";
  return out.str();
}

}  // namespace indigenous::pipeline
