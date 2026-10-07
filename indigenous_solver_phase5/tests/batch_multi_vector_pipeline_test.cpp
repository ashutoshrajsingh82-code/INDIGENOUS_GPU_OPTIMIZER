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

bool near(double a, double b) {
  return std::abs(a - b) < 1e-12;
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

  UnifiedGpuSolverPipeline::VectorBatch rhs_batch{
      {5.0, 2.0},
      {2.0, 1.0}};
  UnifiedGpuSolverPipeline::VectorBatch entering_batch{
      {2.0, 1.0},
      {4.0, 2.0}};

  UnifiedGpuSolverPipeline::VectorBatch ftran_solutions;
  if (!check(pipeline.batch_ftran(entering_batch, ftran_solutions),
             "batch FTRAN")) return 1;
  if (!check(ftran_solutions.size() == 2, "batch FTRAN count")) return 1;
  if (!check(near(ftran_solutions[0][0], 0.5) &&
             near(ftran_solutions[0][1], 1.0),
             "batch FTRAN vector 0")) return 1;
  if (!check(near(ftran_solutions[1][0], 1.0) &&
             near(ftran_solutions[1][1], 2.0),
             "batch FTRAN vector 1")) return 1;

  UnifiedGpuSolverPipeline::VectorBatch btran_solutions;
  if (!check(pipeline.batch_btran(rhs_batch, btran_solutions),
             "batch BTRAN")) return 1;
  if (!check(btran_solutions.size() == 2, "batch BTRAN count")) return 1;
  if (!check(near(btran_solutions[0][0], 2.5) &&
             near(btran_solutions[0][1], -0.5),
             "batch BTRAN vector 0")) return 1;
  if (!check(near(btran_solutions[1][0], 1.0) &&
             near(btran_solutions[1][1], 0.0),
             "batch BTRAN vector 1")) return 1;

  UnifiedGpuSolverPipeline::VectorBatch reduced_costs;
  if (!check(pipeline.batch_price(btran_solutions, reduced_costs),
             "batch pricing")) return 1;
  if (!check(reduced_costs.size() == 2, "batch pricing count")) return 1;
  if (!check(near(reduced_costs[0][0], -2.0) &&
             near(reduced_costs[0][1], 2.0),
             "batch pricing vector 0")) return 1;
  if (!check(near(reduced_costs[1][0], 1.0) &&
             near(reduced_costs[1][1], 3.0),
             "batch pricing vector 1")) return 1;

  UnifiedGpuSolverPipeline::BatchIterationResult result;
  if (!check(pipeline.batch_coordinate_iteration(
                 rhs_batch, entering_batch, result),
             "batch coordinated iteration")) return 1;
  if (!check(result.success, "batch coordinated success")) return 1;
  if (!check(result.dual.size() == 2 &&
             result.reduced_costs.size() == 2 &&
             result.direction.size() == 2,
             "batch coordinated result counts")) return 1;
  if (!check(near(result.dual[0][0], 2.5) &&
             near(result.dual[0][1], -0.5),
             "coordinated dual vector 0")) return 1;
  if (!check(near(result.reduced_costs[0][0], -2.0) &&
             near(result.reduced_costs[0][1], 2.0),
             "coordinated pricing vector 0")) return 1;
  if (!check(near(result.direction[0][0], 0.5) &&
             near(result.direction[0][1], 1.0),
             "coordinated direction vector 0")) return 1;
  if (!check(near(result.direction[1][0], 1.0) &&
             near(result.direction[1][1], 2.0),
             "coordinated direction vector 1")) return 1;

  const auto report = pipeline.report();
  if (!check(report.batch_available, "batch API available")) return 1;
  if (!check(std::string(report.batch_backend) == "CPU-BATCH",
             "CPU batch backend")) return 1;
  if (!check(!report.gpu_active, "CUDA inactive on Intel")) return 1;
  if (!check(report.batch_calls == 4, "four batch calls")) return 1;
  if (!check(report.batch_vectors_processed == 8,
             "eight vectors processed")) return 1;
  if (!check(report.ftran_calls == 4 &&
             report.btran_calls == 4 &&
             report.pricing_calls == 4,
             "underlying operation counters")) return 1;
  if (!check(report.workspace_persistent, "persistent workspace")) return 1;

  std::cout << "Phase 5.6 batch / multi-vector operations: PASS\n";
  std::cout << "Batch calls: " << report.batch_calls
            << " | Vectors processed: " << report.batch_vectors_processed << "\n";
  std::cout << "FTRAN: " << report.ftran_calls
            << " | BTRAN: " << report.btran_calls
            << " | Pricing: " << report.pricing_calls << "\n";
  std::cout << "Backend: " << report.batch_backend
            << " | Workspace persistent: "
            << (report.workspace_persistent ? "YES" : "NO") << "\n";
  return 0;
}
