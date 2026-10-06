#pragma once

#include <cstddef>
#include <utility>
#include <vector>

namespace indigenous::gpu {

// Reusable basis-solve backend. The factorization is supplied in the
// row-oriented form P*A = L*U. Rows contain (column, value) pairs.
// L is unit lower triangular; U is upper triangular.
// perm[k] is the original row stored at factorized row k.
class BasisSolveWorkspace {
public:
  using Index = std::size_t;
  using Real = double;
  using SparseRow = std::vector<std::pair<Index, Real>>;
  using SparseRows = std::vector<SparseRow>;

  BasisSolveWorkspace() = default;
  ~BasisSolveWorkspace() = default;

  BasisSolveWorkspace(const BasisSolveWorkspace&) = delete;
  BasisSolveWorkspace& operator=(const BasisSolveWorkspace&) = delete;
  BasisSolveWorkspace(BasisSolveWorkspace&&) noexcept = default;
  BasisSolveWorkspace& operator=(BasisSolveWorkspace&&) noexcept = default;

  bool initialize(const SparseRows& lower,
                 const SparseRows& upper,
                 const std::vector<Real>& diagonal,
                 const std::vector<Index>& permutation,
                 Real pivot_tolerance = 1e-12);

  bool ftran(const std::vector<Real>& rhs,
             std::vector<Real>& solution);

  bool btran(const std::vector<Real>& rhs,
             std::vector<Real>& solution);

  bool valid() const noexcept { return valid_; }
  bool gpu_enabled() const noexcept { return false; }
  Index size() const noexcept { return n_; }

  double last_ftran_ms() const noexcept { return last_ftran_ms_; }
  double last_btran_ms() const noexcept { return last_btran_ms_; }

private:
  SparseRows lower_;
  SparseRows upper_;
  std::vector<Real> diagonal_;
  std::vector<Index> permutation_;
  std::vector<Real> workspace_;
  Index n_ = 0;
  Real tolerance_ = 1e-12;
  bool valid_ = false;
  double last_ftran_ms_ = 0.0;
  double last_btran_ms_ = 0.0;
};

} // namespace indigenous::gpu
