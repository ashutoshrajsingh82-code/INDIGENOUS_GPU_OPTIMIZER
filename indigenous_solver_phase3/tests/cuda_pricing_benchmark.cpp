#include "gpu/sparse_pricing.hpp"

#if defined(INDIGENOUS_PHASE3_CUDA)
#include <cuda_runtime.h>
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

namespace {
struct BenchmarkData {
  std::vector<std::size_t> offsets;
  std::vector<std::size_t> rows;
  std::vector<double> values;
  std::vector<double> objective;
  std::vector<double> dual;
};

BenchmarkData make_problem(std::size_t rows, std::size_t columns,
                           std::size_t nnz_per_column) {
  BenchmarkData data;
  data.offsets.resize(columns + 1, 0);
  data.objective.resize(columns);
  data.dual.resize(rows);

  std::mt19937_64 rng(26119);
  std::uniform_real_distribution<double> value_dist(-1.0, 1.0);
  std::uniform_int_distribution<std::size_t> row_dist(0, rows - 1);

  data.rows.reserve(columns * nnz_per_column);
  data.values.reserve(columns * nnz_per_column);

  for(std::size_t i = 0; i < rows; ++i)
    data.dual[i] = value_dist(rng);

  for(std::size_t j = 0; j < columns; ++j) {
    data.offsets[j] = data.rows.size();
    data.objective[j] = value_dist(rng);
    for(std::size_t k = 0; k < nnz_per_column; ++k) {
      data.rows.push_back(row_dist(rng));
      data.values.push_back(value_dist(rng));
    }
  }
  data.offsets[columns] = data.rows.size();
  return data;
}

bool close_enough(const std::vector<double>& a,
                  const std::vector<double>& b,
                  double tolerance,
                  double& max_error) {
  if(a.size() != b.size()) return false;
  max_error = 0.0;
  for(std::size_t i = 0; i < a.size(); ++i) {
    max_error = std::max(max_error, std::abs(a[i] - b[i]));
    if(max_error > tolerance) return false;
  }
  return true;
}
}

int main() {
  constexpr std::size_t rows = 4096;
  constexpr std::size_t columns = 8192;
  constexpr std::size_t nnz_per_column = 24;
  constexpr int repeats = 10;

  const BenchmarkData data = make_problem(rows, columns, nnz_per_column);

  indigenous::gpu::SparsePricingWorkspace cpu_workspace;
  if(!cpu_workspace.initialize(
         data.offsets, data.rows, data.values, data.objective)) {
    std::cerr << "CPU workspace initialization failed\n";
    return 1;
  }

  std::vector<double> cpu_result;
  double cpu_total_ms = 0.0;
  for(int i = 0; i < repeats; ++i) {
    const auto start = std::chrono::steady_clock::now();
    if(!cpu_workspace.compute(data.dual, cpu_result)) {
      std::cerr << "CPU pricing failed\n";
      return 1;
    }
    cpu_total_ms += std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
  }

  std::cout << std::fixed << std::setprecision(4);
  std::cout << "CUDA pricing benchmark\n";
  std::cout << "Rows: " << rows
            << " | Columns: " << columns
            << " | NNZ: " << data.values.size()
            << " | Repeats: " << repeats << "\n";
  std::cout << "CPU avg compute_ms: " << cpu_total_ms / repeats << "\n";

  if(!indigenous::gpu::available()) {
    std::cout << "CUDA backend: UNAVAILABLE\n";
    std::cout << "GPU benchmark skipped (CPU fallback is active).\n";
    return 0;
  }

#if defined(INDIGENOUS_PHASE3_CUDA)
  int device_count = 0;
  if(cudaGetDeviceCount(&device_count) != cudaSuccess || device_count <= 0) {
    std::cout << "CUDA runtime: no usable device detected\n";
    return 0;
  }
  cudaDeviceProp device{};
  if(cudaGetDeviceProperties(&device, 0) != cudaSuccess) {
    std::cerr << "Failed to query CUDA device properties\n";
    return 1;
  }
  std::cout << "GPU: " << device.name << "\n";
  std::cout << "Compute capability: " << device.major << "." << device.minor << "\n";
  std::cout << "Global memory MB: "
            << static_cast<double>(device.totalGlobalMem) / (1024.0 * 1024.0) << "\n";
  std::cout << "CUDA runtime: " << CUDART_VERSION << "\n";
#endif

  indigenous::gpu::SparsePricingWorkspace gpu_workspace;
  if(!gpu_workspace.initialize(
         data.offsets, data.rows, data.values, data.objective)) {
    std::cerr << "CUDA workspace initialization failed\n";
    return 1;
  }

  std::vector<double> gpu_result;
  double gpu_total_ms = 0.0;
  double h2d_total_ms = 0.0;
  double kernel_total_ms = 0.0;
  double d2h_total_ms = 0.0;

  for(int i = 0; i < repeats; ++i) {
    if(!gpu_workspace.compute(data.dual, gpu_result)) {
      std::cerr << "CUDA pricing failed\n";
      return 1;
    }
    gpu_total_ms += gpu_workspace.last_compute_ms();
    h2d_total_ms += gpu_workspace.last_host_to_device_ms();
    kernel_total_ms += gpu_workspace.last_kernel_ms();
    d2h_total_ms += gpu_workspace.last_device_to_host_ms();
  }

  double max_error = 0.0;
  const bool match = close_enough(cpu_result, gpu_result, 1e-10, max_error);

  std::cout << "CUDA backend: AVAILABLE\n";
  std::cout << "Workspace init_ms: "
            << gpu_workspace.last_initialize_ms() << "\n";
  std::cout << "GPU avg compute_ms: " << gpu_total_ms / repeats << "\n";
  std::cout << "GPU avg H2D_ms: " << h2d_total_ms / repeats << "\n";
  std::cout << "GPU avg kernel_ms: " << kernel_total_ms / repeats << "\n";
  std::cout << "GPU avg D2H_ms: " << d2h_total_ms / repeats << "\n";
  std::cout << "CPU/GPU max abs error: " << max_error << "\n";
  std::cout << "Numerical agreement: " << (match ? "PASS" : "FAIL") << "\n";

  if(!match) return 1;

  const double cpu_avg = cpu_total_ms / repeats;
  const double gpu_avg = gpu_total_ms / repeats;
  std::cout << "GPU pricing speedup: "
            << (gpu_avg > 0.0 ? cpu_avg / gpu_avg : 0.0) << "x\n";
  return 0;
}
