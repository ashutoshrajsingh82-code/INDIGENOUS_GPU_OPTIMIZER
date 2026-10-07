#include "pipeline/production_solver.hpp"

#include <utility>

namespace indigenous::pipeline {

ProductionSolver::ProductionSolver(Options options)
    : options_(std::move(options)),
      simplex_(options_.simplex),
      pipeline_(options_.pipeline) {}

bool ProductionSolver::model_to_pipeline_pricing(
    const solver::LinearModel& model,
    std::vector<std::size_t>& offsets,
    std::vector<std::size_t>& rows,
    std::vector<Real>& values,
    std::vector<Real>& objective) {
  const auto& matrix = model.A;
  offsets.clear();
  offsets.reserve(matrix.column_pointers().size());
  for (const auto value : matrix.column_pointers())
    offsets.push_back(static_cast<std::size_t>(value));

  rows.clear();
  rows.reserve(matrix.row_indices().size());
  for (const auto row : matrix.row_indices())
    rows.push_back(static_cast<std::size_t>(row));

  values = matrix.values();

  objective.clear();
  objective.reserve(model.variables.size());
  for (const auto& variable : model.variables)
    objective.push_back(variable.objective);

  return offsets.size() == model.variables.size() + 1 &&
         rows.size() == values.size();
}

bool ProductionSolver::preflight_pipeline(const solver::LinearModel& model) {
  std::vector<std::size_t> offsets, rows;
  std::vector<Real> values, objective;
  report_.pipeline_preflight = true;

  if (!model_to_pipeline_pricing(model, offsets, rows, values, objective)) {
    report_.pipeline_preflight_passed = false;
    report_.message = "Phase 5 pipeline preflight: model CSC conversion failed.";
    return false;
  }

  if (!pipeline_.initialize_pricing(offsets, rows, values, objective)) {
    report_.pipeline_preflight_passed = false;
    report_.message = "Phase 5 pipeline preflight: pricing initialization failed.";
    pipeline_report_ = pipeline_.report();
    return false;
  }

  std::vector<Real> dual(model.constraints.size(), 0.0);
  std::vector<Real> reduced_costs;
  const bool priced = pipeline_.price(dual, reduced_costs);

  pipeline_report_ = pipeline_.report();
  report_.pipeline_preflight_passed =
      priced && reduced_costs.size() == model.variables.size() &&
      pipeline_report_.numerical_stable;

  if (!report_.pipeline_preflight_passed) {
    report_.message =
        "Phase 5 pipeline preflight: numerical/backend validation failed.";
    return false;
  }

  return true;
}

solver::SolveResult ProductionSolver::solve(const solver::LinearModel& model) {
  report_ = {};
  report_.execution_backend = "CPU";
  report_.pipeline_pricing_backend = "UNKNOWN";

  if (options_.enable_pipeline_preflight)
    preflight_pipeline(model);

  auto result = simplex_.solve(model);

  report_.solved = true;
  report_.optimal = result.status == solver::SolveStatus::Optimal;
  report_.iterations = result.iterations;
  report_.objective = result.objective_value;
  report_.primal_residual = result.primal_residual;
  report_.dual_residual = result.dual_residual;
  report_.simplex_pricing_backend = result.statistics.pricing_backend.c_str();
  report_.execution_backend = result.statistics.pricing_backend.c_str();

  pipeline_report_ = pipeline_.report();
  report_.pipeline_pricing_backend = pipeline_report_.pricing_backend;
  report_.gpu_active = pipeline_report_.gpu_active;
  report_.pipeline_numerical_checks = pipeline_report_.numerical_checks;
  report_.pipeline_numerical_failures = pipeline_report_.numerical_failures;
  report_.message = result.message;

  return result;
}

bool ProductionSolver::pipeline_preflight_passed() const noexcept {
  return report_.pipeline_preflight_passed;
}

const UnifiedGpuSolverPipeline::Report&
ProductionSolver::pipeline_report() const noexcept {
  return pipeline_report_;
}

const ProductionSolver::Report& ProductionSolver::report() const noexcept {
  return report_;
}

}  // namespace indigenous::pipeline
