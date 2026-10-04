#pragma once
#include <unordered_map>
#include <vector>
#include "solver/common/types.hpp"

namespace solver {

// Sparse basis factorization used by the revised simplex layer.
//
// The factorization maintains sparse row maps for L and U. After a full
// factorization, column replacements are represented as product-form eta
// updates. The eta chain is periodically discarded by refactorizing the
// current basis, keeping solve cost and numerical drift bounded.
class SparseLU {
public:
  bool factorize(const std::vector<std::vector<Real>>& a,
                 Real pivot_tolerance=1e-12);

  bool solve(const std::vector<Real>& b, std::vector<Real>& x) const;
  bool solve_transpose(const std::vector<Real>& b,
                       std::vector<Real>& x) const;

  // Apply a basis-column replacement. direction is B^{-1} a_enter and
  // leaving_row identifies the replaced basis column. The base LU remains
  // unchanged; the replacement is stored as an eta transformation.
  bool update(const std::vector<Real>& direction, Index leaving_row,
              Real pivot_tolerance=1e-12);

  std::size_t update_count() const { return etas_.size(); }
  Index size() const { return n_; }
  Real min_pivot() const { return min_pivot_; }
  std::size_t l_nonzeros() const;
  std::size_t u_nonzeros() const;

private:
  using Row = std::unordered_map<Index, Real>;

  struct EtaUpdate {
    Index pivot_row=0;
    Real pivot=0;
    std::vector<Real> direction;
  };

  Index n_=0;
  Real tol_=1e-12;
  Real min_pivot_=0;

  // After factorization: P*A = L*U.
  std::vector<Row> l_;
  std::vector<Row> u_;

  // perm_[k] is the original row now stored at factorized row k.
  std::vector<Index> perm_;

  // Product-form updates after the most recent full factorization.
  std::vector<EtaUpdate> etas_;
};

} // namespace solver
