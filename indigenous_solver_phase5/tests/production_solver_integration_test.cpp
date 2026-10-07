#include <iostream>
#include "pipeline/production_solver.hpp"

int main() {
  solver::LinearModel model;
  model.name = "phase5_production_smoke";
  model.minimize = true;
  model.variables.resize(2);
  model.variables[0].name = "x";
  model.variables[1].name = "y";
  model.variables[0].objective = -3.0;
  model.variables[1].objective = -2.0;

  model.constraints.resize(1);
  model.constraints[0].name = "capacity";
  model.constraints[0].lower_bound = -solver::kInfinity;
  model.constraints[0].upper_bound = 2.0;

  model.A = solver::CscMatrix(
      1, 2, {1.0, 1.0}, {0, 0}, {0, 1, 2});

  indigenous::pipeline::ProductionSolver::Options options;
  options.enable_pipeline_preflight = true;
  indigenous::pipeline::ProductionSolver production(options);

  const auto result = production.solve(model);
  const auto& report = production.report();

  if (!report.solved ||
      !report.optimal ||
      !report.pipeline_preflight ||
      !report.pipeline_preflight_passed) {
    std::cerr << "FAIL: production integration report\n"
              << " solved=" << report.solved
              << " optimal=" << report.optimal
              << " status=" << solver::to_string(result.status)
              << " pipeline_preflight=" << report.pipeline_preflight
              << " pipeline_preflight_passed=" << report.pipeline_preflight_passed
              << " message=" << report.message << "\n";
    return 1;
  }

  if (result.primal.size() != 2) {
    std::cerr << "FAIL: production primal result\n";
    return 1;
  }

  std::cout << "Phase 5.11 production solver integration: PASS\n";
  std::cout << "Status: " << solver::to_string(result.status)
            << " | Objective: " << result.objective_value
            << " | Iterations: " << result.iterations << "\n";
  std::cout << "Pipeline preflight: PASS"
            << " | Pricing backend: " << report.pipeline_pricing_backend
            << " | Execution: " << report.execution_backend << "\n";
  std::cout << "GPU active: " << (report.gpu_active ? "YES" : "NO")
            << " | Numerical failures: " << report.pipeline_numerical_failures
            << "\n";
  return 0;
}
