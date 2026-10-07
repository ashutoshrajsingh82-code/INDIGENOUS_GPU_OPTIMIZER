#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "pipeline/unified_gpu_solver_pipeline.hpp"

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
  assert(pipeline.initialize_basis(columns, 2));
  assert(pipeline.initialize_pricing(offsets, rows, values, objective));
  assert(pipeline.initialized());

  // Canonical simplex coordination:
  // 1) B^T y = c_B (BTRAN)
  // 2) r = c - A^T y (pricing)
  // 3) B d = a_enter (FTRAN)
  std::vector<double> dual;
  std::vector<double> reduced_costs;
  std::vector<double> direction;

  assert(pipeline.coordinate_iteration(
      {5.0, 2.0}, {2.0, 1.0}, dual, reduced_costs, direction));

  assert(dual.size() == 2);
  assert(std::abs(dual[0] - 2.5) < 1e-12);
  assert(std::abs(dual[1] + 0.5) < 1e-12);

  assert(reduced_costs.size() == 2);
  assert(std::abs(reduced_costs[0] - 1.0) < 1e-12);
  assert(std::abs(reduced_costs[1] - 3.0) < 1e-12);

  assert(direction.size() == 2);
  assert(std::abs(direction[0] - 0.5) < 1e-12);
  assert(std::abs(direction[1] - 1.0) < 1e-12);

  // Repeat the full sequence to verify the same persistent workspace is
  // reused instead of constructing per-operation buffers.
  assert(pipeline.coordinate_iteration(
      {5.0, 2.0}, {2.0, 1.0}, dual, reduced_costs, direction));

  const auto report = pipeline.report();
  assert(std::string(report.basis_backend) == "CPU");
  assert(std::string(report.pricing_backend) == "CPU");
  assert(!report.basis_gpu_active);
  assert(!report.pricing_gpu_active);
  assert(!report.gpu_active);
  assert(report.coordination_calls == 2);
  assert(report.btran_calls == 2);
  assert(report.pricing_calls == 2);
  assert(report.ftran_calls == 2);
  assert(report.workspace_persistent);
  assert(report.workspace_allocations == 4);
  assert(report.workspace_reuses >= 8);

  std::cout << "Phase 5.3 GPU pricing + FTRAN/BTRAN coordination: PASS\n";
  std::cout << "Coordination calls: " << report.coordination_calls
            << " | BTRAN: " << report.btran_calls
            << " | Pricing: " << report.pricing_calls
            << " | FTRAN: " << report.ftran_calls << "\n";
  std::cout << "Basis backend: " << report.basis_backend
            << " | Pricing backend: " << report.pricing_backend
            << " | Execution: " << (report.gpu_active ? "GPU" : "CPU") << "\n";
  std::cout << "Workspace allocations: " << report.workspace_allocations
            << " | Reuses: " << report.workspace_reuses
            << " | Persistent: " << (report.workspace_persistent ? "YES" : "NO") << "\n";
  return 0;
}
