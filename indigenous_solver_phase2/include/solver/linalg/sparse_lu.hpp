#pragma once
#include <unordered_map>
#include <utility>
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

  // Factorize directly from sparse column storage. This avoids materializing
  // a dense basis matrix during simplex refactorization.
  bool factorize_sparse_columns(
      const std::vector<std::vector<std::pair<Index, Real>>>& columns,
      Index dimension, Real pivot_tolerance=1e-12);

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
  double last_eta_forward_ms() const { return last_eta_forward_ms_; }
  double last_eta_transpose_ms() const { return last_eta_transpose_ms_; }
  double last_factor_load_ms() const { return last_factor_load_ms_; }
  double last_factor_pivot_ms() const { return last_factor_pivot_ms_; }
  double last_factor_elimination_ms() const { return last_factor_elimination_ms_; }
  std::size_t last_factor_elimination_affected_rows() const { return last_factor_elimination_affected_rows_; }
  std::size_t last_factor_elimination_row_scan_checks() const { return last_factor_elimination_row_scan_checks_; }
  std::size_t last_factor_elimination_pivot_entries() const { return last_factor_elimination_pivot_entries_; }
  std::size_t last_factor_elimination_hash_finds() const { return last_factor_elimination_hash_finds_; }
  std::size_t last_factor_elimination_hash_inserts() const { return last_factor_elimination_hash_inserts_; }
  std::size_t last_factor_elimination_hash_erases() const { return last_factor_elimination_hash_erases_; }

private:
  using Row = std::unordered_map<Index, Real>;
  using SparseEntry = std::pair<Index, Real>;

  void rebuild_solve_cache();

  struct EtaUpdate {
    Index pivot_row=0;
    Real pivot=0;
    std::vector<Real> direction;
  };

  Index n_=0;
  Real tol_=1e-12;
  Real min_pivot_=0;
  mutable double last_eta_forward_ms_=0;
  mutable double last_eta_transpose_ms_=0;
  mutable double last_factor_load_ms_=0;
  mutable double last_factor_pivot_ms_=0;
  mutable double last_factor_elimination_ms_=0;
  mutable std::size_t last_factor_elimination_affected_rows_=0;
  mutable std::size_t last_factor_elimination_row_scan_checks_=0;
  mutable std::size_t last_factor_elimination_pivot_entries_=0;
  mutable std::size_t last_factor_elimination_hash_finds_=0;
  mutable std::size_t last_factor_elimination_hash_inserts_=0;
  mutable std::size_t last_factor_elimination_hash_erases_=0;

  // After factorization: P*A = L*U.
  std::vector<Row> l_;
  std::vector<Row> u_;

  // Immutable contiguous copies used by FTRAN/BTRAN between refactorizations.
  // Factorization and eta updates continue to use the hash-map rows above.
  std::vector<std::vector<SparseEntry>> l_solve_rows_;
  std::vector<std::vector<SparseEntry>> u_solve_rows_;
  std::vector<Real> u_diagonal_;

  // perm_[k] is the original row now stored at factorized row k.
  std::vector<Index> perm_;

  // Product-form updates after the most recent full factorization.
  std::vector<EtaUpdate> etas_;

  // Reused workspace for the final row-permutation step in BTRAN. Keeping
  // this outside solve_transpose() avoids allocating a full vector on every
  // simplex iteration.
  mutable std::vector<Real> transpose_workspace_;
};

} // namespace solver
