#pragma once

#include <cstddef>
#include <memory>

#include "basis/basis_factorization.hpp"

namespace indigenous::gpu {

class CudaBasisWorkspace {
public:
  using Index = indigenous::basis::BasisFactorization::Index;
  using Real = indigenous::basis::BasisFactorization::Real;

  CudaBasisWorkspace() = default;
  ~CudaBasisWorkspace();

  CudaBasisWorkspace(const CudaBasisWorkspace&) = delete;
  CudaBasisWorkspace& operator=(const CudaBasisWorkspace&) = delete;
  CudaBasisWorkspace(CudaBasisWorkspace&& other) noexcept;
  CudaBasisWorkspace& operator=(CudaBasisWorkspace&& other) noexcept;

  // Uploads a factorized basis P*A=L*U to device memory.
  // This phase intentionally performs representation/upload only.
  bool initialize(const indigenous::basis::BasisFactorization& factorization);

  void release() noexcept;

  bool valid() const noexcept { return valid_; }
  bool device_ready() const noexcept { return device_ready_; }
  Index size() const noexcept { return n_; }

  std::size_t lower_nnz() const noexcept { return lower_nnz_; }
  std::size_t upper_nnz() const noexcept { return upper_nnz_; }
  double last_upload_ms() const noexcept { return last_upload_ms_; }

private:
  void* d_lower_offsets_ = nullptr;
  void* d_lower_columns_ = nullptr;
  void* d_lower_values_ = nullptr;
  void* d_upper_offsets_ = nullptr;
  void* d_upper_columns_ = nullptr;
  void* d_upper_values_ = nullptr;
  void* d_diagonal_ = nullptr;
  void* d_permutation_ = nullptr;

  Index n_ = 0;
  std::size_t lower_nnz_ = 0;
  std::size_t upper_nnz_ = 0;
  double last_upload_ms_ = 0.0;
  bool valid_ = false;
  bool device_ready_ = false;
};

}  // namespace indigenous::gpu
