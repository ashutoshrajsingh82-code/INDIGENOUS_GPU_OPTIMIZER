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
  std::vector<double> ftran;
  assert(pipeline.ftran({5.0, 2.0}, ftran));
  assert(std::abs(ftran[0] - 2.0) < 1e-12);
  assert(std::abs(ftran[1] - 2.0) < 1e-12);
  std::vector<double> btran;
  assert(pipeline.btran({5.0, 2.0}, btran));
  assert(std::abs(btran[0] - 2.5) < 1e-12);
  assert(std::abs(btran[1] + 0.5) < 1e-12);
  std::vector<double> reduced_costs;
  assert(pipeline.price({1.0, 2.0}, reduced_costs));
  assert(std::abs(reduced_costs[0] - 1.0) < 1e-12);
  assert(std::abs(reduced_costs[1] - 1.0) < 1e-12);
  assert(pipeline.update({0.5, 0.0}, 0));
  const auto report = pipeline.report();
  assert(std::string(report.basis_backend) == "CPU");
  assert(std::string(report.pricing_backend) == "CPU");
  assert(!report.gpu_active);
  assert(report.ftran_calls == 1 && report.btran_calls == 1 &&
         report.pricing_calls == 1 && report.update_count == 1);
  std::cout << "Phase 5.1 unified GPU solver pipeline: PASS\n";
  std::cout << "Basis backend: " << report.basis_backend
            << " | Pricing backend: " << report.pricing_backend
            << " | Execution: " << (report.gpu_active ? "GPU" : "CPU") << "\n";
  return 0;
}
