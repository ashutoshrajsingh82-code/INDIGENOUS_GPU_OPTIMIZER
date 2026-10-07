#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <vector>

#include "gpu/sparse_pricing.hpp"
#include "gpu/vector_ops.hpp"

namespace {

bool close(double a, double b, double tol = 1e-10) {
  const double scale = 1.0 + std::max(std::abs(a), std::abs(b));
  return std::abs(a - b) <= tol * scale;
}

double cpu_pricing_reference(
    std::size_t column,
    const std::vector<std::size_t>& offsets,
    const std::vector<std::size_t>& rows,
    const std::vector<double>& values,
    const std::vector<double>& objective,
    const std::vector<double>& dual) {
  double value = objective[column];
  for (std::size_t k = offsets[column]; k < offsets[column + 1]; ++k)
    value -= values[k] * dual[rows[k]];
  return value;
}

}  // namespace

int main() {
  const bool gpu_available = indigenous::gpu::available();
  const char* backend = indigenous::gpu::backend_name();

  std::cout << "Phase 6 GPU hardware validation\n";
  std::cout << "Runtime backend: " << backend << "\n";

  if (!gpu_available) {
    std::cout << "GPU runtime: NOT AVAILABLE\n";
    std::cout << "Validation result: SKIP (CPU-only host; no NVIDIA CUDA device)\n";
    return 0;
  }

  // First validate the CUDA vector path with deterministic data.
  std::vector<float> x(4096, 1.25f);
  std::vector<float> y(4096, 2.0f);
  if (!indigenous::gpu::axpy(2.0f, x, y)) {
    std::cerr << "FAIL: CUDA AXPY operation\n";
    return 1;
  }
  for (float value : y) {
    if (std::abs(value - 4.5f) > 1e-5f) {
      std::cerr << "FAIL: CUDA AXPY numerical result\n";
      return 1;
    }
  }

  // Build a deterministic sparse CSC pricing workload.
  constexpr std::size_t rows_count = 2048;
  constexpr std::size_t columns_count = 2048;
  constexpr std::size_t entries_per_column = 16;
  constexpr std::size_t iterations = 100;

  std::vector<std::size_t> offsets(columns_count + 1, 0);
  std::vector<std::size_t> rows;
  std::vector<double> values;
  std::vector<double> objective(columns_count);
  std::vector<double> dual(rows_count);

  rows.reserve(columns_count * entries_per_column);
  values.reserve(columns_count * entries_per_column);

  for (std::size_t r = 0; r < rows_count; ++r)
    dual[r] = 0.001 * static_cast<double>((r % 97) + 1);

  for (std::size_t c = 0; c < columns_count; ++c) {
    objective[c] = 1.0 + 0.0001 * static_cast<double>(c);
    offsets[c] = rows.size();
    for (std::size_t k = 0; k < entries_per_column; ++k) {
      rows.push_back((c * 17 + k * 31) % rows_count);
      values.push_back(0.01 * static_cast<double>((k % 11) + 1));
    }
  }
  offsets[columns_count] = rows.size();

  indigenous::gpu::SparsePricingWorkspace pricing;
  if (!pricing.initialize(offsets, rows, values, objective)) {
    std::cerr << "FAIL: CUDA pricing workspace initialization\n";
    return 1;
  }

  std::vector<double> reduced_costs;
  for (std::size_t i = 0; i < iterations; ++i) {
    if (!pricing.compute(dual, reduced_costs)) {
      std::cerr << "FAIL: CUDA pricing computation\n";
      return 1;
    }
  }

  if (reduced_costs.size() != columns_count) {
    std::cerr << "FAIL: pricing output dimension\n";
    return 1;
  }

  for (std::size_t c = 0; c < columns_count; ++c) {
    const double expected =
        cpu_pricing_reference(c, offsets, rows, values, objective, dual);
    if (!close(reduced_costs[c], expected)) {
      std::cerr << "FAIL: pricing numerical validation at column " << c << "\n";
      return 1;
    }
  }

  const double total_ms = pricing.last_compute_ms() * static_cast<double>(iterations);
  const double per_iteration_ms = pricing.last_compute_ms();

  std::cout << "GPU runtime: READY\n";
  std::cout << "CUDA vector validation: PASS\n";
  std::cout << "CUDA pricing validation: PASS\n";
  std::cout << "Pricing workload: " << rows_count << "x" << columns_count
            << " | NNZ: " << rows.size() << "\n";
  std::cout << "Iterations: " << iterations
            << " | Last pricing call: " << per_iteration_ms << " ms\n";
  std::cout << "Reported repeated-work estimate: " << total_ms << " ms\n";
  std::cout << "Validation result: PASS\n";
  return 0;
}
