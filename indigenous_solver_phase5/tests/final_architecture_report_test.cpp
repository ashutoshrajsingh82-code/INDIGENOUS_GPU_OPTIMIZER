#include <iostream>

#include "pipeline/final_architecture_report.hpp"

int main() {
  solver::LinearModel model;
  model.name = "phase5_final_architecture_smoke";
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
  model.A = solver::CscMatrix(1, 2, {1.0, 1.0}, {0, 0}, {0, 1, 2});

  indigenous::pipeline::ProductionSolver production;
  const auto result = production.solve(model);
  const auto snapshot =
      indigenous::pipeline::FinalArchitectureReport::build(
          production.pipeline_report(), production.report());

  if (result.primal.size() != 2) {
    std::cerr << "FAIL: primal result\n";
    return 1;
  }
  if (!indigenous::pipeline::FinalArchitectureReport::production_ready(snapshot)) {
    std::cerr << "FAIL: production readiness\n";
    return 1;
  }
  if (snapshot.execution_backend != "CPU" ||
      snapshot.gpu_active ||
      snapshot.basis_gpu_active ||
      snapshot.pricing_gpu_active ||
      !snapshot.pipeline_preflight_passed ||
      !snapshot.numerical_stable) {
    std::cerr << "FAIL: CPU architecture snapshot\n";
    return 1;
  }
  if (snapshot.async_backend != "CPU-ASYNC" ||
      snapshot.batch_backend != "CPU-BATCH") {
    std::cerr << "FAIL: CPU backend reporting\n";
    return 1;
  }

  std::cout << "Phase 5.12 final architecture/reporting: PASS\n";
  std::cout << "Production ready: YES | Execution: "
            << snapshot.execution_backend
            << " | GPU runtime ready: "
            << (indigenous::pipeline::FinalArchitectureReport::gpu_ready(snapshot)
                    ? "YES" : "NO")
            << "\n";
  std::cout << "Pipeline preflight: PASS | Numerical stable: YES"
            << " | Workspace persistent: "
            << (snapshot.workspace_persistent ? "YES" : "NO") << "\n";
  std::cout << "Bottleneck: " << snapshot.bottleneck
            << " | Share: " << snapshot.bottleneck_percent << "%\n";
  return 0;
}
