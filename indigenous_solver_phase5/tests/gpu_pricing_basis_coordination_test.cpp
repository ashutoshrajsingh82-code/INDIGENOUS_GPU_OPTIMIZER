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
  if (!check(pipeline.initialize_basis(columns, 2), "basis initialization")) return 1;
  if (!check(pipeline.initialize_pricing(offsets, rows, values, objective),
             "pricing initialization")) return 1;
  if (!check(pipeline.initialized(), "pipeline initialized")) return 1;

  // Canonical simplex coordination:
  // 1) B^T y = btran_rhs (BTRAN)
  // 2) r = c - A^T y (pricing)
  // 3) B d = a_enter (FTRAN)
  std::vector<double> dual;
  std::vector<double> reduced_costs;
  std::vector<double> direction;

  if (!check(pipeline.coordinate_iteration(
                 {5.0, 2.0}, {2.0, 1.0},
                 dual, reduced_costs, direction),
             "first coordinated iteration")) return 1;

  if (!check(dual.size() == 2, "dual size")) return 1;
  if (!check(std::abs(dual[0] - 2.5) < 1e-12, "dual[0]")) return 1;
  if (!check(std::abs(dual[1] + 0.5) < 1e-12, "dual[1]")) return 1;

  // With y = [2.5, -0.5], r = c - A^T y = [-2, 2].
  if (!check(reduced_costs.size() == 2, "reduced-cost size")) return 1;
  if (!check(std::abs(reduced_costs[0] + 2.0) < 1e-12,
             "reduced_cost[0]")) return 1;
  if (!check(std::abs(reduced_costs[1] - 2.0) < 1e-12,
             "reduced_cost[1]")) return 1;

  if (!check(direction.size() == 2, "FTRAN direction size")) return 1;
  if (!check(std::abs(direction[0] - 0.5) < 1e-12, "direction[0]")) return 1;
  if (!check(std::abs(direction[1] - 1.0) < 1e-12, "direction[1]")) return 1;

  // Repeat the complete sequence to verify persistent workspace reuse.
  if (!check(pipeline.coordinate_iteration(
                 {5.0, 2.0}, {2.0, 1.0},
                 dual, reduced_costs, direction),
             "second coordinated iteration")) return 1;

  const auto report = pipeline.report();
  if (!check(std::string(report.basis_backend) == "CPU",
             "CPU basis backend")) return 1;
  if (!check(std::string(report.pricing_backend) == "CPU",
             "CPU pricing backend")) return 1;
  if (!check(!report.basis_gpu_active, "basis GPU inactive on Intel")) return 1;
  if (!check(!report.pricing_gpu_active, "pricing GPU inactive on Intel")) return 1;
  if (!check(!report.gpu_active, "aggregate GPU inactive on Intel")) return 1;
  if (!check(report.coordination_calls == 2, "two coordination calls")) return 1;
  if (!check(report.btran_calls == 2, "two BTRAN calls")) return 1;
  if (!check(report.pricing_calls == 2, "two pricing calls")) return 1;
  if (!check(report.ftran_calls == 2, "two FTRAN calls")) return 1;
  if (!check(report.workspace_persistent, "persistent workspace")) return 1;
  if (!check(report.workspace_allocations == 3,
             "three initial operation-buffer allocations")) return 1;
  if (!check(report.workspace_reuses >= 4,
             "repeated operations reuse workspace")) return 1;

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
