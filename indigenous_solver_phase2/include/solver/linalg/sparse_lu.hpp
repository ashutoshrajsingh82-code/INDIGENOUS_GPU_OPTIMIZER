#pragma once
#include <unordered_map>
#include <vector>
#include "solver/common/types.hpp"

namespace solver {

// Sparse basis factorization used by the revised simplex layer.
//
// The factorization maintains sparse row maps for L and U. Partial pivoting
// is performed on each column. The implementation deliberately refactorizes
// the basis after a pivot; update schemes such as Forrest-Tomlin remain a
// later performance optimization.
class SparseLU {
public:
  bool factorize(const std::vector<std::vector<Real>>& a,
                 Real pivot_tolerance=1e-12);

  bool solve(const std::vector<Real>& b, std::vector<Real>& x) const;
  bool solve_transpose(const std::vector<Real>& b,
                       std::vector<Real>& x) const;

  Index size() const { return n_; }
  Real min_pivot() const { return min_pivot_; }

private:
  using Row = std::unordered_map<Index, Real>;

  Index n_=0;
  Real tol_=1e-12;
  Real min_pivot_=0;

  // After factorization: P*A = L*U.
  std::vector<Row> l_;
  std::vector<Row> u_;

  // perm_[k] is the original row now stored at factorized row k.
  std::vector<Index> perm_;
};

} // namespace solver
