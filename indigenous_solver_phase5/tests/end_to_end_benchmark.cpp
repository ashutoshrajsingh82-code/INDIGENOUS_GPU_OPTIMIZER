#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include "pipeline/unified_gpu_solver_pipeline.hpp"

namespace {
using Clock = std::chrono::steady_clock;
double ms(const Clock::time_point a, const Clock::time_point b) {
  return std::chrono::duration<double, std::milli>(b - a).count();
}
bool check(bool ok, const char* message) {
  if (!ok) std::cerr << "FAIL: " << message << "\n";
  return ok;
}
std::size_t arg_size(int argc, char** argv, int index, std::size_t fallback) {
  if (argc <= index) return fallback;
  try { return static_cast<std::size_t>(std::stoull(argv[index])); }
  catch (...) { return fallback; }
}
}

int main(int argc, char** argv) {
  const std::size_t dimension = arg_size(argc, argv, 1, 64);
  const std::size_t iterations = arg_size(argc, argv, 2, 100);
  const std::size_t batch_size = arg_size(argc, argv, 3, 8);
  if (dimension == 0 || iterations == 0 || batch_size == 0) return 2;

  using Pipeline = indigenous::pipeline::UnifiedGpuSolverPipeline;
  Pipeline pipeline;
  Pipeline::SparseColumns basis(dimension);
  for (std::size_t i = 0; i < dimension; ++i)
    basis[i] = {{i, 1.0 + 0.001 * static_cast<double>(i)}};

  std::vector<std::size_t> offsets(dimension + 1, 0);
  std::vector<std::size_t> rows(dimension);
  std::vector<double> values(dimension);
  std::vector<double> objective(dimension);
  for (std::size_t j = 0; j < dimension; ++j) {
    offsets[j] = j;
    rows[j] = j;
    values[j] = 0.5 + 0.001 * static_cast<double>(j);
    objective[j] = 1.0 + 0.01 * static_cast<double>(j);
  }
  offsets[dimension] = dimension;

  if (!check(pipeline.initialize_basis(basis, dimension), "basis initialization") ||
      !check(pipeline.initialize_pricing(offsets, rows, values, objective),
             "pricing initialization")) return 1;

  std::vector<double> rhs(dimension, 1.0);
  std::vector<double> entering(dimension, 0.5);
  std::vector<double> dual, reduced, direction;

  const auto start = Clock::now();
  for (std::size_t i = 0; i < iterations; ++i) {
    if (!pipeline.coordinate_iteration(rhs, entering, dual, reduced, direction))
      return 1;
  }
  const auto end = Clock::now();

  Pipeline::VectorBatch rhs_batch(batch_size, rhs);
  Pipeline::VectorBatch entering_batch(batch_size, entering);
  Pipeline::BatchIterationResult batch_result;
  const auto batch_start = Clock::now();
  if (!pipeline.batch_coordinate_iteration(rhs_batch, entering_batch, batch_result))
    return 1;
  const auto batch_end = Clock::now();

  auto async = pipeline.coordinate_iteration_async(rhs, entering);
  const auto async_start = Clock::now();
  if (!pipeline.wait_async(async)) return 1;
  const auto async_end = Clock::now();

  const auto report = pipeline.report();
  std::cout << std::fixed << std::setprecision(6);
  std::cout << "Phase 5.9 end-to-end pipeline benchmark: PASS\n";
  std::cout << "Dimension: " << dimension
            << " | Iterations: " << iterations
            << " | Batch size: " << batch_size << "\n";
  std::cout << "Coordinate total_ms: " << ms(start, end)
            << " | per_iteration_ms: " << ms(start, end) / iterations << "\n";
  std::cout << "Batch total_ms: " << ms(batch_start, batch_end)
            << " | Async total_ms: " << ms(async_start, async_end) << "\n";
  std::cout << "Basis backend: " << report.basis_backend
            << " | Pricing backend: " << report.pricing_backend
            << " | Execution: " << (report.gpu_active ? "GPU" : "CPU") << "\n";
  std::cout << "Async backend: " << report.async_backend
            << " | Batch backend: " << report.batch_backend << "\n";
  std::cout << "Workspace persistent: " << (report.workspace_persistent ? "YES" : "NO")
            << " | Numerical stable: " << (report.numerical_stable ? "YES" : "NO") << "\n";
  std::cout << "FTRAN: " << report.ftran_calls
            << " | BTRAN: " << report.btran_calls
            << " | Pricing: " << report.pricing_calls
            << " | Coordination: " << report.coordination_calls << "\n";
  std::cout << "Batch calls: " << report.batch_calls
            << " | Vectors processed: " << report.batch_vectors_processed
            << " | Async submitted: " << report.async_submitted
            << " | Async completed: " << report.async_completed << "\n";
  return report.numerical_stable ? 0 : 1;
}
