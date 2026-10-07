#include "basis/basis_validation.hpp"
#include "basis/gpu_cpu_validation.hpp"
#include "basis/basis_factorization.hpp"
#include "gpu/cuda_basis_workspace.hpp"

#include <cuda_runtime.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

using Real = double;
using Index = std::int64_t;
using Matrix = std::vector<std::vector<Real>>;

Matrix multiply(const Matrix& a, const Matrix& b) {
  const std::size_t n = a.size();
  Matrix c(n, std::vector<Real>(n, 0.0));
  for (std::size_t i = 0; i < n; ++i)
    for (std::size_t k = 0; k < n; ++k)
      for (std::size_t j = 0; j < n; ++j)
        c[i][j] += a[i][k] * b[k][j];
  return c;
}

Matrix transpose(const Matrix& a) {
  Matrix t(a.size(), std::vector<Real>(a.size(), 0.0));
  for (std::size_t i = 0; i < a.size(); ++i)
    for (std::size_t j = 0; j < a.size(); ++j)
      t[i][j] = a[j][i];
  return t;
}

std::vector<std::vector<std::pair<Index, Real>>> to_columns(const Matrix& a) {
  std::vector<std::vector<std::pair<Index, Real>>> columns(a.size());
  for (Index j = 0; j < static_cast<Index>(a.size()); ++j)
    for (Index i = 0; i < static_cast<Index>(a.size()); ++i) {
      const Real value = a[static_cast<std::size_t>(i)]
                          [static_cast<std::size_t>(j)];
      if (std::abs(value) > 1e-14)
        columns[static_cast<std::size_t>(j)].push_back({i, value});
    }
  return columns;
}

std::vector<Real> matvec(const Matrix& a, const std::vector<Real>& x) {
  std::vector<Real> y(a.size(), 0.0);
  for (std::size_t i = 0; i < a.size(); ++i)
    for (std::size_t j = 0; j < a.size(); ++j)
      y[i] += a[i][j] * x[j];
  return y;
}

double max_error(const std::vector<Real>& a, const std::vector<Real>& b) {
  if (a.size() != b.size()) return INFINITY;
  double error = 0.0;
  for (std::size_t i = 0; i < a.size(); ++i)
    error = std::max(error, std::abs(a[i] - b[i]));
  return error;
}

}  // namespace

int main() {
  int device_count = 0;
  if (cudaGetDeviceCount(&device_count) != cudaSuccess || device_count <= 0) {
    std::cout << "Phase 4.10 GPU/CPU numerical validation: SKIP (no CUDA device)
";
    return 0;
  }

  indigenous::basis::BasisFactorization factorization;
  factorization.lower = {
      {},
      {{0, 2.0}},
      {{0, -1.0}, {1, 3.0}}
  };
  factorization.upper = {
      {{1, 1.0}},
      {{2, 4.0}},
      {}
  };
  factorization.diagonal = {2.0, 3.0, 5.0};
  factorization.permutation = {0, 2, 1};

  if (!factorization.structurally_valid()) return 1;

  const Matrix l = {
      {1.0, 0.0, 0.0},
      {2.0, 1.0, 0.0},
      {-1.0, 3.0, 1.0}
  };
  const Matrix u = {
      {2.0, 1.0, 0.0},
      {0.0, 3.0, 4.0},
      {0.0, 0.0, 5.0}
  };
  const Matrix pa = multiply(l, u);
  Matrix a(3, std::vector<Real>(3, 0.0));
  for (std::size_t i = 0; i < 3; ++i)
    a[factorization.permutation[i]] = pa[i];

  const auto columns = to_columns(a);
  auto cpu = indigenous::basis::make_cpu_basis_solver();
  if (!cpu->initialize(columns, 3)) return 1;

  indigenous::gpu::CudaBasisWorkspace gpu;
  if (!gpu.initialize(factorization)) return 1;

  const std::vector<Real> f_rhs = {1.75, -4.0, 9.25};
  const std::vector<Real> b_rhs = {-2.0, 5.5, 3.25};

  std::vector<Real> f_cpu, f_gpu, b_cpu, b_gpu;
  if (!cpu->ftran(f_rhs, f_cpu) || !gpu.ftran(f_rhs, f_gpu) ||
      !cpu->btran(b_rhs, b_cpu) || !gpu.btran(b_rhs, b_gpu)) return 1;

  const auto f_cmp = indigenous::basis::validation::compare_vectors(f_cpu, f_gpu);
  const auto b_cmp = indigenous::basis::validation::compare_vectors(b_cpu, b_gpu);

  const auto f_cpu_res =
      indigenous::basis::validation::ftran_residual(columns, 3, f_cpu, f_rhs);
  const auto b_cpu_res =
      indigenous::basis::validation::btran_residual(columns, 3, b_cpu, b_rhs);
  const auto f_gpu_res =
      indigenous::basis::validation::ftran_residual(columns, 3, f_gpu, f_rhs);
  const auto b_gpu_res =
      indigenous::basis::validation::btran_residual(columns, 3, b_gpu, b_rhs);

  std::vector<Real> f_gpu_repeat, b_gpu_repeat;
  if (!gpu.ftran(f_rhs, f_gpu_repeat) || !gpu.btran(b_rhs, b_gpu_repeat))
    return 1;

  const double f_repeat = max_error(f_gpu, f_gpu_repeat);
  const double b_repeat = max_error(b_gpu, b_gpu_repeat);

  std::vector<Real> cpu_dual_u, cpu_dual_v;
  if (!cpu->ftran(f_rhs, cpu_dual_u) || !cpu->btran(b_rhs, cpu_dual_v))
    return 1;
  const double cpu_duality = std::abs(
      std::inner_product(cpu_dual_u.begin(), cpu_dual_u.end(), b_rhs.begin(), 0.0) -
      std::inner_product(f_rhs.begin(), f_rhs.end(), cpu_dual_v.begin(), 0.0));

  constexpr double tolerance = 1e-10;
  const bool residuals_ok =
      f_cpu_res.finite && b_cpu_res.finite && f_gpu_res.finite && b_gpu_res.finite &&
      indigenous::basis::validation::residual_within_tolerance(f_cpu_res.infinity_norm, 1.0) &&
      indigenous::basis::validation::residual_within_tolerance(b_cpu_res.infinity_norm, 1.0) &&
      indigenous::basis::validation::residual_within_tolerance(f_gpu_res.infinity_norm, 1.0) &&
      indigenous::basis::validation::residual_within_tolerance(b_gpu_res.infinity_norm, 1.0);

  const bool repeated_ok = f_repeat <= tolerance && b_repeat <= tolerance;
  const bool passed = f_cmp.within_tolerance && b_cmp.within_tolerance &&
                      residuals_ok && repeated_ok &&
                      cpu_duality <= tolerance;

  if (!passed) {
    std::cerr << "Phase 4.10 GPU/CPU numerical validation: FAIL
"
              << "  FTRAN CPU/GPU max abs error: " << f_cmp.max_absolute_error << "
"
              << "  BTRAN CPU/GPU max abs error: " << b_cmp.max_absolute_error << "
"
              << "  FTRAN GPU residual: " << f_gpu_res.infinity_norm << "
"
              << "  BTRAN GPU residual: " << b_gpu_res.infinity_norm << "
"
              << "  FTRAN repeat error: " << f_repeat << "
"
              << "  BTRAN repeat error: " << b_repeat << "
"
              << "  CPU duality error: " << cpu_duality << "
";
    return 1;
  }

  std::cout << "Phase 4.10 GPU/CPU numerical validation: PASS
"
            << "  FTRAN CPU/GPU max abs error: " << f_cmp.max_absolute_error << "
"
            << "  BTRAN CPU/GPU max abs error: " << b_cmp.max_absolute_error << "
"
            << "  FTRAN CPU residual: " << f_cpu_res.infinity_norm << "
"
            << "  FTRAN GPU residual: " << f_gpu_res.infinity_norm << "
"
            << "  BTRAN CPU residual: " << b_cpu_res.infinity_norm << "
"
            << "  BTRAN GPU residual: " << b_gpu_res.infinity_norm << "
"
            << "  FTRAN repeat error: " << f_repeat << "
"
            << "  BTRAN repeat error: " << b_repeat << "
"
            << "  CPU duality error: " << cpu_duality << "
";
  return 0;
}
