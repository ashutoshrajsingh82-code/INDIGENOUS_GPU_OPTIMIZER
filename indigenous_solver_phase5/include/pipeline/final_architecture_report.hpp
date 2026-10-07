#pragma once

#include <cstddef>
#include <string>

#include "pipeline/production_solver.hpp"

namespace indigenous::pipeline {

class FinalArchitectureReport final {
public:
  struct Snapshot {
    std::string execution_backend = "CPU";
    std::string basis_backend = "NONE";
    std::string pricing_backend = "UNKNOWN";
    std::string adaptive_basis_backend = "CPU";
    std::string adaptive_pricing_backend = "CPU";
    std::string async_backend = "CPU-ASYNC";
    std::string batch_backend = "CPU-BATCH";
    std::string sparse_strategy = "direct sparse CPU";
    std::string sparse_scale = "SMALL";
    std::string bottleneck = "NONE";
    std::string status = "NOT_RUN";
    std::string message;

    bool production_solved = false;
    bool production_optimal = false;
    bool pipeline_initialized = false;
    bool pipeline_preflight = false;
    bool pipeline_preflight_passed = false;

    bool gpu_active = false;
    bool basis_gpu_active = false;
    bool pricing_gpu_active = false;
    bool adaptive_gpu_eligible = false;
    bool basis_gpu_recommended = false;
    bool pricing_gpu_recommended = false;
    bool async_gpu_capable = false;

    bool workspace_persistent = false;
    bool numerical_stable = true;
    bool fallback_active = false;
    bool sparse_large_scale = false;

    std::size_t iterations = 0;
    std::size_t ftran_calls = 0;
    std::size_t btran_calls = 0;
    std::size_t pricing_calls = 0;
    std::size_t coordination_calls = 0;
    std::size_t updates = 0;
    std::size_t workspace_allocations = 0;
    std::size_t workspace_reuses = 0;
    std::size_t adaptive_decisions = 0;
    std::size_t async_submitted = 0;
    std::size_t async_completed = 0;
    std::size_t batch_calls = 0;
    std::size_t batch_vectors_processed = 0;
    std::size_t sparse_rows = 0;
    std::size_t sparse_columns = 0;
    std::size_t sparse_nonzeros = 0;
    std::size_t recommended_batch_vectors = 1;
    std::size_t chunk_columns = 1;
    std::size_t numerical_checks = 0;
    std::size_t numerical_failures = 0;
    std::size_t fallback_count = 0;

    double objective = 0.0;
    double primal_residual = 0.0;
    double dual_residual = 0.0;
    double maximum_residual = 0.0;
    double bottleneck_percent = 0.0;
  };

  static Snapshot build(const UnifiedGpuSolverPipeline::Report& pipeline,
                        const ProductionSolver::Report& production);

  static bool production_ready(const Snapshot& snapshot) noexcept;
  static bool gpu_ready(const Snapshot& snapshot) noexcept;
  static std::string to_text(const Snapshot& snapshot);
};

}  // namespace indigenous::pipeline
