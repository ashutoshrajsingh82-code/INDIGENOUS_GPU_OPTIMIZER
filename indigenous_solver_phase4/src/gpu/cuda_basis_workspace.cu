#include "gpu/cuda_basis_workspace.hpp"

#include <cuda_runtime.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace indigenous::gpu {
namespace {

template <typename T>
void free_device(T*& pointer) noexcept {
  if (pointer != nullptr) {
    cudaFree(pointer);
    pointer = nullptr;
  }
}

bool copy_to_device(void** destination, const void* source, std::size_t bytes) {
  if (bytes == 0) {
    *destination = nullptr;
    return true;
  }
  if (cudaMalloc(destination, bytes) != cudaSuccess) return false;
  if (cudaMemcpy(*destination, source, bytes, cudaMemcpyHostToDevice) !=
      cudaSuccess) {
    cudaFree(*destination);
    *destination = nullptr;
    return false;
  }
  return true;
}

template <typename T>
bool copy_vector(void** destination, const std::vector<T>& values) {
  return copy_to_device(destination, values.data(),
                        values.size() * sizeof(T));
}

}  // namespace

CudaBasisWorkspace::~CudaBasisWorkspace() {
  release();
}

CudaBasisWorkspace::CudaBasisWorkspace(CudaBasisWorkspace&& other) noexcept {
  *this = std::move(other);
}

CudaBasisWorkspace& CudaBasisWorkspace::operator=(
    CudaBasisWorkspace&& other) noexcept {
  if (this == &other) return *this;
  release();

  d_lower_offsets_ = other.d_lower_offsets_;
  d_lower_columns_ = other.d_lower_columns_;
  d_lower_values_ = other.d_lower_values_;
  d_upper_offsets_ = other.d_upper_offsets_;
  d_upper_columns_ = other.d_upper_columns_;
  d_upper_values_ = other.d_upper_values_;
  d_diagonal_ = other.d_diagonal_;
  d_permutation_ = other.d_permutation_;

  n_ = other.n_;
  lower_nnz_ = other.lower_nnz_;
  upper_nnz_ = other.upper_nnz_;
  last_upload_ms_ = other.last_upload_ms_;
  valid_ = other.valid_;
  device_ready_ = other.device_ready_;

  other.d_lower_offsets_ = nullptr;
  other.d_lower_columns_ = nullptr;
  other.d_lower_values_ = nullptr;
  other.d_upper_offsets_ = nullptr;
  other.d_upper_columns_ = nullptr;
  other.d_upper_values_ = nullptr;
  other.d_diagonal_ = nullptr;
  other.d_permutation_ = nullptr;
  other.n_ = 0;
  other.lower_nnz_ = 0;
  other.upper_nnz_ = 0;
  other.valid_ = false;
  other.device_ready_ = false;
  other.last_upload_ms_ = 0.0;
  return *this;
}

void CudaBasisWorkspace::release() noexcept {
  free_device(d_lower_offsets_);
  free_device(d_lower_columns_);
  free_device(d_lower_values_);
  free_device(d_upper_offsets_);
  free_device(d_upper_columns_);
  free_device(d_upper_values_);
  free_device(d_diagonal_);
  free_device(d_permutation_);

  n_ = 0;
  lower_nnz_ = 0;
  upper_nnz_ = 0;
  last_upload_ms_ = 0.0;
  valid_ = false;
  device_ready_ = false;
}

bool CudaBasisWorkspace::initialize(
    const indigenous::basis::BasisFactorization& factorization) {
  release();

  if (!factorization.structurally_valid()) return false;

  int device_count = 0;
  if (cudaGetDeviceCount(&device_count) != cudaSuccess || device_count <= 0)
    return false;

  const Index n = factorization.size();
  std::vector<std::int64_t> lower_offsets(static_cast<std::size_t>(n) + 1, 0);
  std::vector<std::int64_t> upper_offsets(static_cast<std::size_t>(n) + 1, 0);
  std::vector<std::int64_t> lower_columns;
  std::vector<std::int64_t> upper_columns;
  std::vector<Real> lower_values;
  std::vector<Real> upper_values;

  for (Index row = 0; row < n; ++row) {
    const auto& lower_row = factorization.lower[static_cast<std::size_t>(row)];
    const auto& upper_row = factorization.upper[static_cast<std::size_t>(row)];

    lower_offsets[static_cast<std::size_t>(row + 1)] =
        lower_offsets[static_cast<std::size_t>(row)] +
        static_cast<std::int64_t>(lower_row.size());
    upper_offsets[static_cast<std::size_t>(row + 1)] =
        upper_offsets[static_cast<std::size_t>(row)] +
        static_cast<std::int64_t>(upper_row.size());

    for (const auto& [column, value] : lower_row) {
      lower_columns.push_back(column);
      lower_values.push_back(value);
    }
    for (const auto& [column, value] : upper_row) {
      upper_columns.push_back(column);
      upper_values.push_back(value);
    }
  }

  const auto start = std::chrono::steady_clock::now();

  if (!copy_vector(&d_lower_offsets_, lower_offsets) ||
      !copy_vector(&d_lower_columns_, lower_columns) ||
      !copy_vector(&d_lower_values_, lower_values) ||
      !copy_vector(&d_upper_offsets_, upper_offsets) ||
      !copy_vector(&d_upper_columns_, upper_columns) ||
      !copy_vector(&d_upper_values_, upper_values) ||
      !copy_vector(&d_diagonal_, factorization.diagonal) ||
      !copy_vector(&d_permutation_, factorization.permutation)) {
    release();
    return false;
  }

  if (cudaDeviceSynchronize() != cudaSuccess) {
    release();
    return false;
  }

  n_ = n;
  lower_nnz_ = lower_columns.size();
  upper_nnz_ = upper_columns.size();
  last_upload_ms_ = std::chrono::duration<double, std::milli>(
      std::chrono::steady_clock::now() - start).count();
  valid_ = true;
  device_ready_ = true;
  return true;
}

}  // namespace indigenous::gpu
