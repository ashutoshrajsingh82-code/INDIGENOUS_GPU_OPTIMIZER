#include <iomanip>
#include <iostream>
#include <string>
#include "pipeline/production_solver.hpp"
#include "solver/io/lp_reader.hpp"
#include "solver/io/mps_reader.hpp"
#include "solver/model/solution_validator.hpp"

using indigenous::pipeline::ProductionSolver;

static bool load(const std::string& f, solver::LinearModel& m, std::string& e) {
  const auto p = f.find_last_of('.');
  const auto ext = p == std::string::npos ? "" : f.substr(p + 1);
  if (ext == "mps" || ext == "MPS") return solver::read_mps(f, m, e);
  return solver::read_lp(f, m, e);
}

int main(int argc, char** argv) {
  if (argc < 3 || std::string(argv[1]) != "solve") {
    std::cerr << "Usage: indigenous_phase5_production_cli solve <model.lp|model.mps> [--max-iters N]\n";
    return 2;
  }

  solver::LinearModel model;
  std::string error;
  if (!load(argv[2], model, error)) {
    std::cerr << "ERROR: " << error << "\n";
    return 1;
  }

  ProductionSolver::Options options;
  for (int i = 3; i + 1 < argc; ++i) {
    if (std::string(argv[i]) == "--max-iters")
      options.simplex.max_iterations = std::stoull(argv[++i]);
  }

  ProductionSolver production(options);
  const auto result = production.solve(model);
  const auto& report = production.report();

  std::cout << "Phase 5.11 Production Solver\n";
  std::cout << "Status: " << solver::to_string(result.status) << "\n";
  std::cout << "Objective: " << std::setprecision(12) << result.objective_value << "\n";
  std::cout << "Iterations: " << result.iterations << "\n";
  std::cout << "Primal residual: " << result.primal_residual << "\n";
  std::cout << "Pipeline preflight: "
            << (report.pipeline_preflight_passed ? "PASS" : "FAIL") << "\n";
  std::cout << "Pipeline pricing backend: " << report.pipeline_pricing_backend << "\n";
  std::cout << "Production execution backend: " << report.execution_backend << "\n";
  std::cout << "GPU active: " << (report.gpu_active ? "YES" : "NO") << "\n";

  if (!result.primal.empty()) {
    const auto certificate = solver::validate_solution(
        model, result.primal, result.objective_value);
    std::cout << "Certificate: " << (certificate.valid ? "PASS" : "FAIL") << "\n";
  }

  return result.status == solver::SolveStatus::Optimal ? 0 : 1;
}
