#include "gpu/cuda_basis_workspace.hpp"

#include <cuda_runtime.h>
#include <cusparse.h>

#include <chrono>
#include <cstdint>
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
  return copy_to_device(destination, values.data(), values.size() * sizeof(T));
}

template <typename T>
T* as(void* pointer) {
  return static_cast<T*>(pointer);
}

bool ok(cudaError_t status) {
  return status == cudaSuccess;
}

bool ok(cusparseStatus_t status) {
  return status == CUSPARSE_STATUS_SUCCESS;
}

__global__ void permute_rhs_kernel(const double* rhs,
                                   const std::int64_t* permutation,
                                   double* permuted,
                                   std::int64_t n) {
  const auto i = static_cast<std::int64_t>(
      blockIdx.x * blockDim.x + threadIdx.x);
  if (i < n) permuted[i] = rhs[permutation[i]];
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
  d_upper_solve_offsets_ = other.d_upper_solve_offsets_;
  d_upper_solve_columns_ = other.d_upper_solve_columns_;
  d_upper_solve_values_ = other.d_upper_solve_values_;
  d_rhs_ = other.d_rhs_;
  d_forward_ = other.d_forward_;
  d_solution_ = other.d_solution_;
  d_diagonal_ = other.d_diagonal_;
  d_permutation_ = other.d_permutation_;

  cusparse_handle_ = other.cusparse_handle_;
  lower_matrix_ = other.lower_matrix_;
  upper_matrix_ = other.upper_matrix_;
  rhs_vector_ = other.rhs_vector_;
  forward_vector_ = other.forward_vector_;
  solution_vector_ = other.solution_vector_;
  lower_spsv_ = other.lower_spsv_;
  upper_spsv_ = other.upper_spsv_;
  lower_buffer_ = other.lower_buffer_;
  upper_buffer_ = other.upper_buffer_;

  n_ = other.n_;
  lower_nnz_ = other.lower_nnz_;
  upper_nnz_ = other.upper_nnz_;
  last_upload_ms_ = other.last_upload_ms_;
  last_ftran_ms_ = other.last_ftran_ms_;
  valid_ = other.valid_;
  device_ready_ = other.device_ready_;

  other.d_lower_offsets_ = nullptr;
  other.d_lower_columns_ = nullptr;
  other.d_lower_values_ = nullptr;
  other.d_upper_offsets_ = nullptr;
  other.d_upper_columns_ = nullptr;
  other.d_upper_values_ = nullptr;
  other.d_upper_solve_offsets_ = nullptr;
  other.d_upper_solve_columns_ = nullptr;
  other.d_upper_solve_values_ = nullptr;
  other.d_rhs_ = nullptr;
  other.d_forward_ = nullptr;
  other.d_solution_ = nullptr;
  other.d_diagonal_ = nullptr;
  other.d_permutation_ = nullptr;

  other.cusparse_handle_ = nullptr;
  other.lower_matrix_ = nullptr;
  other.upper_matrix_ = nullptr;
  other.rhs_vector_ = nullptr;
  other.forward_vector_ = nullptr;
  other.solution_vector_ = nullptr;
  other.lower_spsv_ = nullptr;
  other.upper_spsv_ = nullptr;
  other.lower_buffer_ = nullptr;
  other.upper_buffer_ = nullptr;

  other.n_ = 0;
  other.lower_nnz_ = 0;
  other.upper_nnz_ = 0;
  other.last_upload_ms_ = 0.0;
  other.last_ftran_ms_ = 0.0;
  other.valid_ = false;
  other.device_ready_ = false;
  return *this;
}

void CudaBasisWorkspace::release() noexcept {
  if (auto* descriptor = as<cusparseDnVecDescr_t>(rhs_vector_);
      descriptor != nullptr) {
    cusparseDestroyDnVec(*descriptor);
    delete descriptor;
    rhs_vector_ = nullptr;
  }
  if (auto* descriptor = as<cusparseDnVecDescr_t>(forward_vector_);
      descriptor != nullptr) {
    cusparseDestroyDnVec(*descriptor);
    delete descriptor;
    forward_vector_ = nullptr;
  }
  if (auto* descriptor = as<cusparseDnVecDescr_t>(solution_vector_);
      descriptor != nullptr) {
    cusparseDestroyDnVec(*descriptor);
    delete descriptor;
    solution_vector_ = nullptr;
  }
  if (auto* descriptor = as<cusparseSpMatDescr_t>(lower_matrix_);
      descriptor != nullptr) {
    cusparseDestroySpMat(*descriptor);
    delete descriptor;
    lower_matrix_ = nullptr;
  }
  if (auto* descriptor = as<cusparseSpMatDescr_t>(upper_matrix_);
      descriptor != nullptr) {
    cusparseDestroySpMat(*descriptor);
    delete descriptor;
    upper_matrix_ = nullptr;
  }
  if (auto* descriptor = as<cusparseSpSVDescr_t>(lower_spsv_);
      descriptor != nullptr) {
    cusparseSpSV_destroyDescr(*descriptor);
    delete descriptor;
    lower_spsv_ = nullptr;
  }
  if (auto* descriptor = as<cusparseSpSVDescr_t>(upper_spsv_);
      descriptor != nullptr) {
    cusparseSpSV_destroyDescr(*descriptor);
    delete descriptor;
    upper_spsv_ = nullptr;
  }
  if (auto* handle = as<cusparseHandle_t>(cusparse_handle_);
      handle != nullptr) {
    cusparseDestroy(*handle);
    delete handle;
    cusparse_handle_ = nullptr;
  }

  free_device(d_lower_offsets_);
  free_device(d_lower_columns_);
  free_device(d_lower_values_);
  free_device(d_upper_offsets_);
  free_device(d_upper_columns_);
  free_device(d_upper_values_);
  free_device(d_upper_solve_offsets_);
  free_device(d_upper_solve_columns_);
  free_device(d_upper_solve_values_);
  free_device(d_rhs_);
  free_device(d_forward_);
  free_device(d_solution_);
  free_device(d_diagonal_);
  free_device(d_permutation_);
  free_device(as<void>(lower_buffer_));
  free_device(as<void>(upper_buffer_));

  n_ = 0;
  lower_nnz_ = 0;
  upper_nnz_ = 0;
  last_upload_ms_ = 0.0;
  last_ftran_ms_ = 0.0;
  valid_ = false;
  device_ready_ = false;
}

bool CudaBasisWorkspace::initialize(
    const indigenous::basis::BasisFactorization& factorization) {
  release();

  if (!factorization.structurally_valid()) return false;

  int device_count = 0;
  if (!ok(cudaGetDeviceCount(&device_count)) || device_count <= 0)
    return false;

  const Index n = factorization.size();
  std::vector<std::int64_t> lower_offsets(static_cast<std::size_t>(n) + 1, 0);
  std::vector<std::int64_t> upper_offsets(static_cast<std::size_t>(n) + 1, 0);
  std::vector<std::int64_t> upper_solve_offsets(
      static_cast<std::size_t>(n) + 1, 0);
  std::vector<std::int64_t> lower_columns;
  std::vector<std::int64_t> upper_columns;
  std::vector<Real> lower_values;
  std::vector<Real> upper_values;

  // U must contain its non-unit diagonal for SpSV. The public representation
  // keeps diagonal values separate, while the private solve representation
  // stores them in CSR.
  std::vector<std::int64_t> upper_solve_columns;
  std::vector<Real> upper_solve_values;

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

    upper_solve_columns.push_back(row);
    upper_solve_values.push_back(
        factorization.diagonal[static_cast<std::size_t>(row)]);
    for (const auto& [column, value] : upper_row) {
      upper_solve_columns.push_back(column);
      upper_solve_values.push_back(value);
    }

    upper_solve_offsets[static_cast<std::size_t>(row + 1)] =
        static_cast<std::int64_t>(upper_solve_columns.size());
  }

  const auto start = std::chrono::steady_clock::now();

  if (!copy_vector(&d_lower_offsets_, lower_offsets) ||
      !copy_vector(&d_lower_columns_, lower_columns) ||
      !copy_vector(&d_lower_values_, lower_values) ||
      !copy_vector(&d_upper_offsets_, upper_offsets) ||
      !copy_vector(&d_upper_columns_, upper_columns) ||
      !copy_vector(&d_upper_values_, upper_values) ||
      !copy_vector(&d_upper_solve_offsets_, upper_solve_offsets) ||
      !copy_vector(&d_upper_solve_columns_, upper_solve_columns) ||
      !copy_vector(&d_upper_solve_values_, upper_solve_values) ||
      !copy_vector(&d_diagonal_, factorization.diagonal) ||
      !copy_vector(&d_permutation_, factorization.permutation) ||
      !ok(cudaMalloc(&d_rhs_, static_cast<std::size_t>(n) * sizeof(Real))) ||
      !ok(cudaMalloc(&d_forward_, static_cast<std::size_t>(n) * sizeof(Real))) ||
      !ok(cudaMalloc(&d_solution_, static_cast<std::size_t>(n) * sizeof(Real)))) {
    release();
    return false;
  }

  auto* handle = new cusparseHandle_t{};
  if (!ok(cusparseCreate(handle))) {
    delete handle;
    release();
    return false;
  }
  cusparse_handle_ = handle;

  auto* lower_matrix = new cusparseSpMatDescr_t{};
  if (!ok(cusparseCreateCsr(
          lower_matrix, n, n, static_cast<std::int64_t>(lower_columns.size()),
          d_lower_offsets_, d_lower_columns_, d_lower_values_,
          CUSPARSE_INDEX_64I, CUSPARSE_INDEX_64I, CUSPARSE_INDEX_BASE_ZERO,
          CUDA_R_64F))) {
    delete lower_matrix;
    release();
    return false;
  }
  lower_matrix_ = lower_matrix;

  auto* upper_matrix = new cusparseSpMatDescr_t{};
  if (!ok(cusparseCreateCsr(
          upper_matrix, n, n,
          static_cast<std::int64_t>(upper_solve_columns.size()),
          d_upper_solve_offsets_, d_upper_solve_columns_,
          d_upper_solve_values_, CUSPARSE_INDEX_64I, CUSPARSE_INDEX_64I,
          CUSPARSE_INDEX_BASE_ZERO, CUDA_R_64F))) {
    delete upper_matrix;
    release();
    return false;
  }
  upper_matrix_ = upper_matrix;

  const auto lower_fill = CUSPARSE_FILL_MODE_LOWER;
  const auto lower_diag = CUSPARSE_DIAG_TYPE_UNIT;
  const auto upper_fill = CUSPARSE_FILL_MODE_UPPER;
  const auto upper_diag = CUSPARSE_DIAG_TYPE_NON_UNIT;

  if (!ok(cusparseSpMatSetAttribute(
          *lower_matrix, CUSPARSE_SPMAT_FILL_MODE, &lower_fill,
          sizeof(lower_fill))) ||
      !ok(cusparseSpMatSetAttribute(
          *lower_matrix, CUSPARSE_SPMAT_DIAG_TYPE, &lower_diag,
          sizeof(lower_diag))) ||
      !ok(cusparseSpMatSetAttribute(
          *upper_matrix, CUSPARSE_SPMAT_FILL_MODE, &upper_fill,
          sizeof(upper_fill))) ||
      !ok(cusparseSpMatSetAttribute(
          *upper_matrix, CUSPARSE_SPMAT_DIAG_TYPE, &upper_diag,
          sizeof(upper_diag)))) {
    release();
    return false;
  }

  auto* rhs_vector = new cusparseDnVecDescr_t{};
  auto* forward_vector = new cusparseDnVecDescr_t{};
  auto* solution_vector = new cusparseDnVecDescr_t{};

  if (!ok(cusparseCreateDnVec(rhs_vector, n, d_rhs_, CUDA_R_64F)) ||
      !ok(cusparseCreateDnVec(forward_vector, n, d_forward_, CUDA_R_64F)) ||
      !ok(cusparseCreateDnVec(solution_vector, n, d_solution_, CUDA_R_64F))) {
    delete rhs_vector;
    delete forward_vector;
    delete solution_vector;
    release();
    return false;
  }
  rhs_vector_ = rhs_vector;
  forward_vector_ = forward_vector;
  solution_vector_ = solution_vector;

  auto* lower_spsv = new cusparseSpSVDescr_t{};
  auto* upper_spsv = new cusparseSpSVDescr_t{};
  if (!ok(cusparseSpSV_createDescr(lower_spsv)) ||
      !ok(cusparseSpSV_createDescr(upper_spsv))) {
    delete lower_spsv;
    delete upper_spsv;
    release();
    return false;
  }
  lower_spsv_ = lower_spsv;
  upper_spsv_ = upper_spsv;

  const Real alpha = 1.0;
  std::size_t lower_buffer_size = 0;
  std::size_t upper_buffer_size = 0;

  if (!ok(cusparseSpSV_bufferSize(
          *handle, CUSPARSE_OPERATION_NON_TRANSPOSE, &alpha, *lower_matrix,
          *forward_vector, *solution_vector, CUDA_R_64F,
          CUSPARSE_SPSV_ALG_DEFAULT, *lower_spsv, &lower_buffer_size)) ||
      !ok(cusparseSpSV_bufferSize(
          *handle, CUSPARSE_OPERATION_NON_TRANSPOSE, &alpha, *upper_matrix,
          *solution_vector, *forward_vector, CUDA_R_64F,
          CUSPARSE_SPSV_ALG_DEFAULT, *upper_spsv, &upper_buffer_size))) {
    release();
    return false;
  }

  if (lower_buffer_size != 0 &&
      !ok(cudaMalloc(&lower_buffer_, lower_buffer_size))) {
    release();
    return false;
  }
  if (upper_buffer_size != 0 &&
      !ok(cudaMalloc(&upper_buffer_, upper_buffer_size))) {
    release();
    return false;
  }

  if (!ok(cusparseSpSV_analysis(
          *handle, CUSPARSE_OPERATION_NON_TRANSPOSE, &alpha, *lower_matrix,
          *forward_vector, *solution_vector, CUDA_R_64F,
          CUSPARSE_SPSV_ALG_DEFAULT, *lower_spsv, lower_buffer_)) ||
      !ok(cusparseSpSV_analysis(
          *handle, CUSPARSE_OPERATION_NON_TRANSPOSE, &alpha, *upper_matrix,
          *solution_vector, *forward_vector, CUDA_R_64F,
          CUSPARSE_SPSV_ALG_DEFAULT, *upper_spsv, upper_buffer_))) {
    release();
    return false;
  }

  if (!ok(cudaDeviceSynchronize())) {
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

bool CudaBasisWorkspace::ftran(const std::vector<Real>& rhs,
                               std::vector<Real>& solution) {
  if (!valid_ || !device_ready_ ||
      static_cast<Index>(rhs.size()) != n_) {
    return false;
  }

  const auto start = std::chrono::steady_clock::now();
  const auto bytes = static_cast<std::size_t>(n_) * sizeof(Real);

  if (!ok(cudaMemcpy(d_rhs_, rhs.data(), bytes, cudaMemcpyHostToDevice)))
    return false;

  const int threads = 256;
  const int blocks = static_cast<int>((n_ + threads - 1) / threads);
  permute_rhs_kernel<<<blocks, threads>>>(
      as<const Real>(d_rhs_), as<const std::int64_t>(d_permutation_),
      as<Real>(d_forward_), n_);

  if (!ok(cudaGetLastError())) return false;

  auto* handle = as<cusparseHandle_t>(cusparse_handle_);
  auto* lower_matrix = as<cusparseSpMatDescr_t>(lower_matrix_);
  auto* upper_matrix = as<cusparseSpMatDescr_t>(upper_matrix_);
  auto* forward_vector = as<cusparseDnVecDescr_t>(forward_vector_);
  auto* solution_vector = as<cusparseDnVecDescr_t>(solution_vector_);
  auto* lower_spsv = as<cusparseSpSVDescr_t>(lower_spsv_);
  auto* upper_spsv = as<cusparseSpSVDescr_t>(upper_spsv_);

  const Real alpha = 1.0;
  if (!ok(cusparseSpSV_solve(
          *handle, CUSPARSE_OPERATION_NON_TRANSPOSE, &alpha, *lower_matrix,
          *forward_vector, *solution_vector, CUDA_R_64F,
          CUSPARSE_SPSV_ALG_DEFAULT, *lower_spsv))) {
    return false;
  }

  if (!ok(cusparseSpSV_solve(
          *handle, CUSPARSE_OPERATION_NON_TRANSPOSE, &alpha, *upper_matrix,
          *solution_vector, *forward_vector, CUDA_R_64F,
          CUSPARSE_SPSV_ALG_DEFAULT, *upper_spsv))) {
    return false;
  }

  solution.resize(static_cast<std::size_t>(n_));
  if (!ok(cudaMemcpy(solution.data(), d_forward_, bytes,
                     cudaMemcpyDeviceToHost))) {
    solution.clear();
    return false;
  }

  last_ftran_ms_ = std::chrono::duration<double, std::milli>(
      std::chrono::steady_clock::now() - start).count();
  return true;
}

}  // namespace indigenous::gpu
