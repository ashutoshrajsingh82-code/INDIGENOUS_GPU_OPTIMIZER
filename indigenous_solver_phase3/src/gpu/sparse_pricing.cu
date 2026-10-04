#include "gpu/sparse_pricing.hpp"

#include <cuda_runtime.h>

namespace indigenous::gpu {
namespace {

__global__ void sparse_reduced_costs_kernel(
    const std::size_t* column_offsets,
    const std::size_t* row_indices,
    const double* values,
    const double* objective,
    const double* dual,
    double* reduced_costs,
    std::size_t column_count) {
  const std::size_t j =
      static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
  if(j >= column_count) return;

  double rc = objective[j];
  for(std::size_t p = column_offsets[j]; p < column_offsets[j + 1]; ++p)
    rc -= values[p] * dual[row_indices[p]];
  reduced_costs[j] = rc;
}

} // namespace

bool sparse_reduced_costs(
    const std::vector<std::size_t>& column_offsets,
    const std::vector<std::size_t>& row_indices,
    const std::vector<double>& values,
    const std::vector<double>& objective,
    const std::vector<double>& dual,
    std::vector<double>& reduced_costs) {
  if(column_offsets.empty() || column_offsets.back() != values.size() ||
     row_indices.size() != values.size() ||
     objective.size() + 1 != column_offsets.size()) {
    return false;
  }
  for(std::size_t row : row_indices)
    if(row >= dual.size()) return false;
  if(objective.empty()) {
    reduced_costs.clear();
    return true;
  }

  std::size_t* d_offsets = nullptr;
  std::size_t* d_rows = nullptr;
  double* d_values = nullptr;
  double* d_objective = nullptr;
  double* d_dual = nullptr;
  double* d_result = nullptr;

  const std::size_t offset_bytes = column_offsets.size() * sizeof(std::size_t);
  const std::size_t nnz_bytes = values.size() * sizeof(double);
  const std::size_t row_bytes = row_indices.size() * sizeof(std::size_t);
  const std::size_t column_bytes = objective.size() * sizeof(double);
  const std::size_t dual_bytes = dual.size() * sizeof(double);

  auto cleanup = [&]() {
    cudaFree(d_offsets);
    cudaFree(d_rows);
    cudaFree(d_values);
    cudaFree(d_objective);
    cudaFree(d_dual);
    cudaFree(d_result);
  };

  if(cudaMalloc(&d_offsets, offset_bytes) != cudaSuccess) return false;
  if(!row_indices.empty() && cudaMalloc(&d_rows, row_bytes) != cudaSuccess) {
    cleanup(); return false;
  }
  if(!values.empty() && cudaMalloc(&d_values, nnz_bytes) != cudaSuccess) {
    cleanup(); return false;
  }
  if(cudaMalloc(&d_objective, column_bytes) != cudaSuccess) {
    cleanup(); return false;
  }
  if(!dual.empty() && cudaMalloc(&d_dual, dual_bytes) != cudaSuccess) {
    cleanup(); return false;
  }
  if(cudaMalloc(&d_result, column_bytes) != cudaSuccess) {
    cleanup(); return false;
  }

  bool ok = true;
  ok = ok && cudaMemcpy(d_offsets, column_offsets.data(), offset_bytes,
                        cudaMemcpyHostToDevice) == cudaSuccess;
  if(ok && !row_indices.empty())
    ok = cudaMemcpy(d_rows, row_indices.data(), row_bytes,
                    cudaMemcpyHostToDevice) == cudaSuccess;
  if(ok && !values.empty())
    ok = cudaMemcpy(d_values, values.data(), nnz_bytes,
                    cudaMemcpyHostToDevice) == cudaSuccess;
  if(ok)
    ok = cudaMemcpy(d_objective, objective.data(), column_bytes,
                    cudaMemcpyHostToDevice) == cudaSuccess;
  if(ok && !dual.empty())
    ok = cudaMemcpy(d_dual, dual.data(), dual_bytes,
                    cudaMemcpyHostToDevice) == cudaSuccess;

  if(ok) {
    constexpr unsigned threads = 256;
    const unsigned blocks = static_cast<unsigned>(
        (objective.size() + threads - 1) / threads);
    sparse_reduced_costs_kernel<<<blocks, threads>>>(
        d_offsets, d_rows, d_values, d_objective, d_dual, d_result,
        objective.size());
    ok = cudaGetLastError() == cudaSuccess;
    ok = ok && cudaDeviceSynchronize() == cudaSuccess;
  }

  reduced_costs.resize(objective.size());
  if(ok)
    ok = cudaMemcpy(reduced_costs.data(), d_result, column_bytes,
                    cudaMemcpyDeviceToHost) == cudaSuccess;

  cleanup();
  return ok;
}

} // namespace indigenous::gpu
