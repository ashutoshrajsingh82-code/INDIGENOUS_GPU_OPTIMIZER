#include "basis/basis_solver.hpp"
#include "gpu/cuda_basis_workspace.hpp"

#include <cuda_runtime.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <utility>
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
  const Index n = static_cast<Index>(a.size());
  std::vector<std::vector<std::pair<Index, Real>>> columns(
      static_cast<std::size_t>(n));
  for (Index j = 0; j < n; ++j)
    for (Index i = 0; i < n; ++i) {
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
  double error = 0.0;
  for (std::size_t i = 0; i < a.size(); ++i)
    error = std::max(error, std::abs(a[i] - b[i]));
  return error;
}

}  // namespace

int main() {
  int device_count = 0;
  if (cudaGetDeviceCount(&device_count) != cudaSuccess || device_count <= 0) {
    std::cout << "Phase 4.7 CUDA BTRAN validation: SKIP (no CUDA device)\n";
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

  if (!factorization.structurally_valid()) {
    std::cerr << "Factorization structure: FAIL\n";
    return 1;
  }

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

  // P*A=L*U, so A is obtained by undoing the row permutation.
  Matrix a(3, std::vector<Real>(3, 0.0));
  for (std::size_t i = 0; i < 3; ++i)
    a[factorization.permutation[i]] = pa[i];

  const std::vector<Real> expected = {1.25, -2.0, 0.75};
  const std::vector<Real> rhs = matvec(transpose(a), expected);

  auto cpu = indigenous::basis::make_cpu_basis_solver();
  if (!cpu->initialize(to_columns(a), 3)) {
    std::cerr << "CPU reference initialization: FAIL\n";
    return 1;
  }

  std::vector<Real> cpu_solution;
  if (!cpu->btran(rhs, cpu_solution)) {
    std::cerr << "CPU reference BTRAN: FAIL\n";
    return 1;
  }

  indigenous::gpu::CudaBasisWorkspace workspace;
  if (!workspace.initialize(factorization)) {
    std::cerr << "CUDA BTRAN workspace initialization: FAIL\n";
    return 1;
  }

  std::vector<Real> gpu_solution;
  if (!workspace.btran(rhs, gpu_solution)) {
    std::cerr << "CUDA BTRAN solve: FAIL\n";
    return 1;
  }

  std::vector<Real> gpu_solution_second;
  if (!workspace.btran(rhs, gpu_solution_second)) {
    std::cerr << "CUDA BTRAN repeated solve: FAIL\n";
    return 1;
  }

  const double cpu_error = max_error(cpu_solution, expected);
  const double gpu_error = max_error(gpu_solution, expected);
  const double cross_error = max_error(gpu_solution, cpu_solution);
  const double repeat_error = max_error(gpu_solution, gpu_solution_second);
  const double residual = max_error(matvec(transpose(a), gpu_solution), rhs);

  constexpr double tolerance = 1e-10;
  if (cpu_error > tolerance || gpu_error > tolerance ||
      cross_error > tolerance || repeat_error > tolerance ||
      residual > tolerance) {
    std::cerr << "CUDA BTRAN numerical validation: FAIL\n"
              << "  CPU error: " << cpu_error << "\n"
              << "  GPU error: " << gpu_error << "\n"
              << "  CPU/GPU error: " << cross_error << "\n"
              << "  repeated-call error: " << repeat_error << "\n"
              << "  B^T*x-rhs residual: " << residual << "\n";
    return 1;
  }

  std::cout << "Phase 4.7 CUDA BTRAN validation: PASS\n";
  std::cout << "  CPU error: " << cpu_error << "\n";
  std::cout << "  GPU error: " << gpu_error << "\n";
  std::cout << "  CPU/GPU error: " << cross_error << "\n";
  std::cout << "  B^T*x-rhs residual: " << residual << "\n";
  std::cout << "  repeated-call error: " << repeat_error << "\n";
  std::cout << "  BTRAN ms: " << workspace.last_btran_ms() << "\n";
  return 0;
}
