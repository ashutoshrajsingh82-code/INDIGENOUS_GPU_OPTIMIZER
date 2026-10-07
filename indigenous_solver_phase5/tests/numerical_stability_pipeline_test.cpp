#include <cmath>
#include <iostream>
#include <string>
#include <limits>
#include <vector>

#include "pipeline/numerical_stability_guard.hpp"
#include "pipeline/unified_gpu_solver_pipeline.hpp"

static bool check(bool ok, const char* message) {
  if (!ok) std::cerr << "FAIL: " << message << "\n";
  return ok;
}

int main() {
  using indigenous::pipeline::NumericalStabilityGuard;
  NumericalStabilityGuard guard({1e-9, 1e-12});

  const auto good = guard.validate_input({1.0, -2.0, 3.0});
  if (!check(good.valid, "finite input")) return 1;

  const auto bad_input = guard.validate_input(
      {1.0, std::numeric_limits<double>::quiet_NaN()});
  if (!check(!bad_input.valid &&
             bad_input.failure == NumericalStabilityGuard::Failure::NonFiniteInput,
             "NaN input detection")) return 1;

  const auto bad_output = guard.validate_vector(
      {1.0, std::numeric_limits<double>::infinity()});
  if (!check(!bad_output.valid &&
             bad_output.failure == NumericalStabilityGuard::Failure::NonFiniteOutput,
             "Inf output detection")) return 1;

  const auto residual_ok =
      guard.validate_residual({2.0, 4.0}, {2.0, 4.0}, {1.0, 2.0});
  if (!check(residual_ok.valid && residual_ok.residual == 0.0,
             "zero residual")) return 1;

  const auto residual_bad =
      guard.validate_residual({2.0, 4.1}, {2.0, 4.0}, {1.0, 2.0});
  if (!check(!residual_bad.valid &&
             residual_bad.failure == NumericalStabilityGuard::Failure::ResidualTooLarge,
             "residual threshold")) return 1;

  using Pipeline = indigenous::pipeline::UnifiedGpuSolverPipeline;
  Pipeline::Options options;
  options.enable_numerical_fallback = true;
  Pipeline pipeline(options);

  Pipeline::SparseColumns basis(2);
  basis[0] = {{0, 2.0}};
  basis[1] = {{1, 3.0}};
  const std::vector<std::size_t> offsets{0, 1, 2};
  const std::vector<std::size_t> rows{0, 1};
  const std::vector<double> values{2.0, 3.0};
  const std::vector<double> objective{5.0, 7.0};

  if (!check(pipeline.initialize_basis(basis, 2), "pipeline basis")) return 1;
  if (!check(pipeline.initialize_pricing(offsets, rows, values, objective),
             "pipeline pricing")) return 1;

  std::vector<double> solution;
  if (!check(pipeline.ftran({2.0, 6.0}, solution), "finite FTRAN")) return 1;
  if (!check(solution.size() == 2 &&
             std::abs(solution[0] - 1.0) < 1e-12 &&
             std::abs(solution[1] - 2.0) < 1e-12,
             "FTRAN result")) return 1;

  std::vector<double> reduced;
  if (!check(pipeline.price({1.0, 2.0}, reduced), "finite pricing")) return 1;
  if (!check(reduced.size() == 2 &&
             std::abs(reduced[0] - 3.0) < 1e-12 &&
             std::abs(reduced[1] - 1.0) < 1e-12,
             "pricing result")) return 1;

  if (!check(!pipeline.ftran(
                 {std::numeric_limits<double>::quiet_NaN(), 1.0}, solution),
             "pipeline rejects NaN input")) return 1;

  const auto report = pipeline.report();
  if (!check(report.numerical_checks >= 3, "numerical check accounting")) return 1;
  if (!check(report.numerical_failures >= 1, "numerical failure accounting")) return 1;
  if (!check(std::string(report.last_numerical_failure) == "NONFINITE_INPUT",
             "failure classification")) return 1;

  std::cout << "Phase 5.8 numerical stability and fallback: PASS\n";
  std::cout << "Checks: " << report.numerical_checks
            << " | Failures: " << report.numerical_failures
            << " | Fallbacks: " << report.fallback_count
            << " | Stable: " << (report.numerical_stable ? "YES" : "NO")
            << "\n";
  return 0;
}
