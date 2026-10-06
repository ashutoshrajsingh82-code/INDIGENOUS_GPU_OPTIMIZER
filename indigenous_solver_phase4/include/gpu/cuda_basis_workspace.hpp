#pragma once

#include <cstddef>
#include <memory>
#include <vector>

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

  // Solves B*x = rhs using the uploaded P*A=L*U factorization.
  // The factorization and SpSV analysis remain resident between calls.
  bool ftran(const std::vector<Real>& rhs, std::vector<Real>& solution);

  void release() noexcept;

  bool valid() const noexcept { return valid_; }
  bool device_ready() const noexcept { return device_ready_; }
  Index size() const noexcept { return n_; }

  std::size_t lower_nnz() const noexcept { return lower_nnz_; }
  std::size_t upper_nnz() const noexcept { return upper_nnz_; }
  double last_upload_ms() const noexcept { return last_upload_ms_; }
  double last_ftran_ms() const noexcept { return last_ftran_ms_; }

private:
  void* d_lower_offsets_ = nullptr;
  void* d_lower_columns_ = nullptr;
  void* d_lower_values_ = nullptr;
  void* d_upper_offsets_ = nullptr;
  void* d_upper_columns_ = nullptr;
  void* d_upper_values_ = nullptr;

  // U with its non-unit diagonal included, used by cuSPARSE SpSV.
  void* d_upper_solve_offsets_ = nullptr;
  void* d_upper_solve_columns_ = nullptr;
  void* d_upper_solve_values_ = nullptr;

  void* d_rhs_ = nullptr;
  void* d_forward_ = nullptr;
  void* d_solution_ = nullptr;

  void* cusparse_handle_ = nullptr;
  void* lower_matrix_ = nullptr;
  void* upper_matrix_ = nullptr;
  void* rhs_vector_ = nullptr;
  void* forward_vector_ = nullptr;
  void* solution_vector_ = nullptr;
  void* lower_spsv_ = nullptr;
  void* upper_spsv_ = nullptr;
  void* lower_buffer_ = nullptr;
  void* upper_buffer_ = nullptr;

  void* d_diagonal_ = nullptr;
  void* d_permutation_ = nullptr;

  Index n_ = 0;
  std::size_t lower_nnz_ = 0;
  std::size_t upper_nnz_ = 0;
  double last_upload_ms_ = 0.0;
  double last_ftran_ms_ = 0.0;
  bool valid_ = false;
  bool device_ready_ = false;
};

}  // namespace indigenous::gpu
