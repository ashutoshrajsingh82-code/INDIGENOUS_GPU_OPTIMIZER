#include "gpu/basis_solver.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>

namespace indigenous::gpu {

bool BasisSolveWorkspace::initialize(
    const SparseRows& lower,
    const SparseRows& upper,
    const std::vector<Real>& diagonal,
    const std::vector<Index>& permutation,
    Real pivot_tolerance) {
  const Index n = lower.size();
  if (n == 0 || upper.size() != n || diagonal.size() != n ||
      permutation.size() != n) {
    valid_ = false;
    n_ = 0;
    return false;
  }

  std::vector<bool> seen(n, false);
  for (Index i = 0; i < n; ++i) {
    if (permutation[i] >= n || seen[permutation[i]] ||
        !std::isfinite(diagonal[i]) ||
        std::abs(diagonal[i]) <= pivot_tolerance) {
      valid_ = false;
      n_ = 0;
      return false;
    }
    seen[permutation[i]] = true;
  }

  lower_ = lower;
  upper_ = upper;
  diagonal_ = diagonal;
  permutation_ = permutation;
  workspace_.resize(n);
  n_ = n;
  tolerance_ = std::max<Real>(pivot_tolerance, 0.0);
  valid_ = true;
  return true;
}

bool BasisSolveWorkspace::ftran(
    const std::vector<Real>& rhs,
    std::vector<Real>& solution) {
  if (!valid_ || rhs.size() != n_) return false;

  const auto start = std::chrono::steady_clock::now();
  solution.resize(n_);

  // P*b followed by L*y=P*b. L has an implicit unit diagonal.
  for (Index i = 0; i < n_; ++i) {
    Real value = rhs[permutation_[i]];
    for (const auto& [j, coefficient] : lower_[i]) {
      if (j < i) value -= coefficient * solution[j];
    }
    solution[i] = value;
  }

  // U*x=y.
  for (Index ii = n_; ii-- > 0;) {
    Real value = solution[ii];
    for (const auto& [j, coefficient] : upper_[ii]) {
      if (j > ii) value -= coefficient * solution[j];
    }
    const Real pivot = diagonal_[ii];
    if (!std::isfinite(pivot) || std::abs(pivot) <= tolerance_) {
      valid_ = false;
      return false;
    }
    solution[ii] = value / pivot;
  }

  last_ftran_ms_ = std::chrono::duration<double, std::milli>(
      std::chrono::steady_clock::now() - start).count();
  return true;
}

bool BasisSolveWorkspace::btran(
    const std::vector<Real>& rhs,
    std::vector<Real>& solution) {
  if (!valid_ || rhs.size() != n_) return false;

  const auto start = std::chrono::steady_clock::now();
  solution = rhs;

  // Solve U^T*y=b.
  for (Index i = 0; i < n_; ++i) {
    Real value = solution[i];
    for (const auto& [j, coefficient] : upper_[i]) {
      if (j < i) value -= coefficient * solution[j];
    }
    const Real pivot = diagonal_[i];
    if (!std::isfinite(pivot) || std::abs(pivot) <= tolerance_) {
      valid_ = false;
      return false;
    }
    solution[i] = value / pivot;
  }

  // Solve L^T*z=y. L has unit diagonal.
  for (Index ii = n_; ii-- > 0;) {
    const Real value = solution[ii];
    for (const auto& [j, coefficient] : lower_[ii]) {
      if (j < ii) solution[j] -= coefficient * value;
    }
  }

  // z = P*x, so x[P(i)] = z(i).
  workspace_ = solution;
  for (Index i = 0; i < n_; ++i)
    solution[permutation_[i]] = workspace_[i];

  last_btran_ms_ = std::chrono::duration<double, std::milli>(
      std::chrono::steady_clock::now() - start).count();
  return true;
}

} // namespace indigenous::gpu
