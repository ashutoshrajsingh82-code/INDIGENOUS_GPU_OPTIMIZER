#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "solver/model/linear_model.hpp"
#include "solver/simplex/revised_simplex.hpp"
#include "pipeline/unified_gpu_solver_pipeline.hpp"

namespace indigenous::pipeline {

class ProductionSolver final {
public:
  using Real = solver::Real;

  struct Options {
    solver::RevisedSimplexOptions simplex;
    UnifiedGpuSolverPipeline::Options pipeline;
    bool enable_pipeline_preflight = true;
  };

  struct Report {
    bool solved = false;
    bool optimal = false;
    bool pipeline_preflight = false;
    bool pipeline_preflight_passed = false;
    bool gpu_active = false;
    const char* simplex_pricing_backend = "CPU";
    const char* pipeline_pricing_backend = "UNKNOWN";
    const char* execution_backend = "CPU";
    std::size_t iterations = 0;
    double objective = 0.0;
    double primal_residual = 0.0;
    double dual_residual = 0.0;
    std::size_t pipeline_numerical_checks = 0;
    std::size_t pipeline_numerical_failures = 0;
    std::string message;
  };

  explicit ProductionSolver(Options options = {});

  solver::SolveResult solve(const solver::LinearModel& model);

  bool preflight_pipeline(const solver::LinearModel& model);
  bool pipeline_preflight_passed() const noexcept;
  const UnifiedGpuSolverPipeline::Report& pipeline_report() const noexcept;
  const Report& report() const noexcept;

private:
  static bool model_to_pipeline_pricing(const solver::LinearModel& model,
                                        std::vector<std::size_t>& offsets,
                                        std::vector<std::size_t>& rows,
                                        std::vector<Real>& values,
                                        std::vector<Real>& objective);

  Options options_;
  solver::RevisedSimplexSolver simplex_;
  UnifiedGpuSolverPipeline pipeline_;
  Report report_;
  UnifiedGpuSolverPipeline::Report pipeline_report_;
};

}  // namespace indigenous::pipeline
