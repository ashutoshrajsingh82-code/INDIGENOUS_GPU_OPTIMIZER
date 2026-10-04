#pragma once
#include <vector>
#include "solver/common/types.hpp"

namespace solver {

// Phase 2 basis factorization. The storage is sparse-by-row and refactorization
// is explicit; this gives the simplex layer a stable B/B^T solve interface.
// Forrest-Tomlin updates are deliberately deferred until after correctness.
class SparseLU {
public:
  bool factorize(const std::vector<std::vector<Real>>& a, Real pivot_tolerance=1e-12);
  bool solve(const std::vector<Real>& b, std::vector<Real>& x) const;
  bool solve_transpose(const std::vector<Real>& b, std::vector<Real>& x) const;
  Index size() const { return n_; }
  Real min_pivot() const { return min_pivot_; }
private:
  Index n_=0;
  Real tol_=1e-12, min_pivot_=0;
  std::vector<std::vector<Real>> lu_;
  std::vector<Index> piv_;
};
}
