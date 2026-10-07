#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "pipeline/unified_gpu_solver_pipeline.hpp"

namespace {
bool check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << "\n";
    return false;
  }
  return true;
}
}

int main() {
  using indigenous::pipeline::UnifiedGpuSolverPipeline;

  UnifiedGpuSolverPipeline::SparseColumns columns(2);
  columns[0] = {{0, 2.0}};
  columns[1] = {{0, 1.0}, {1, 1.0}};

  const std::vector<std::size_t> offsets{0, 1, 3};
  const std::vector<std::size_t> rows{0, 0, 1};
  const std::vector<double> values{2.0, 1.0, 1.0};
  const std::vector<double> objective{3.0, 4.0};

  UnifiedGpuSolverPipeline pipeline;
  if (!check(pipeline.initialize_basis(columns, 2), "basis initialization"))
    return 1;
  if (!check(pipeline.initialize_pricing(offsets, rows, values, objective),
             "pricing initialization"))
    return 1;

  auto operation = pipeline.coordinate_iteration_async(
      {5.0, 2.0}, {2.0, 1.0});

  if (!check(operation.task != 0, "async task submission")) return 1;
  if (!check(operation.result != nullptr, "async result state")) return 1;

  if (!check(pipeline.wait_async(operation), "async operation completion"))
    return 1;

  if (!check(operation.result->success, "async operation success")) return 1;
  if (!check(operation.result->dual.size() == 2, "async dual size")) return 1;
  if (!check(std::abs(operation.result->dual[0] - 2.5) < 1e-12,
             "async dual[0]"))
    return 1;
  if (!check(std::abs(operation.result->dual[1] + 0.5) < 1e-12,
             "async dual[1]"))
    return 1;

  if (!check(operation.result->reduced_costs.size() == 2,
             "async reduced-cost size"))
    return 1;
  if (!check(std::abs(operation.result->reduced_costs[0] + 2.0) < 1e-12,
             "async reduced_cost[0]"))
    return 1;
  if (!check(std::abs(operation.result->reduced_costs[1] - 2.0) < 1e-12,
             "async reduced_cost[1]"))
    return 1;

  if (!check(operation.result->direction.size() == 2,
             "async direction size"))
    return 1;
  if (!check(std::abs(operation.result->direction[0] - 0.5) < 1e-12,
             "async direction[0]"))
    return 1;
  if (!check(std::abs(operation.result->direction[1] - 1.0) < 1e-12,
             "async direction[1]"))
    return 1;

  const auto report = pipeline.report();
  if (!check(report.async_available, "async execution available")) return 1;
  if (!check(report.async_submitted == 1, "one async task submitted"))
    return 1;
  if (!check(report.async_completed == 1, "one async task completed"))
    return 1;
  if (!check(report.async_in_flight == 0, "no async tasks in flight"))
    return 1;
  if (!check(std::string(report.async_backend) == "CPU-ASYNC",
             "CPU async backend"))
    return 1;
  if (!check(!report.async_gpu_capable,
             "CUDA async capability inactive on Intel"))
    return 1;
  if (!check(report.ftran_calls == 1 && report.btran_calls == 1 &&
             report.pricing_calls == 1,
             "coordinated operation counters"))
    return 1;

  std::cout << "Phase 5.5 asynchronous pipeline execution: PASS\n";
  std::cout << "Async submitted: " << report.async_submitted
            << " | Completed: " << report.async_completed
            << " | In-flight: " << report.async_in_flight << "\n";
  std::cout << "Basis backend: " << report.basis_backend
            << " | Pricing backend: " << report.pricing_backend
            << " | Async backend: " << report.async_backend << "\n";
  return 0;
}
