#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "basis/basis_solver.hpp"

namespace indigenous::basis {

// Phase 4.9 integration facade used by revised-simplex style iterations.
// The simplex layer owns pivot selection and primal updates; this class owns
// the basis linear algebra backend and keeps the backend-neutral call surface
// limited to initialize/FTRAN/BTRAN/update.
class SimplexBasisBackend final {
public:
  struct Options {
    bool prefer_gpu = true;
    solver::Real pivot_tolerance = 1e-12;
  };

  explicit SimplexBasisBackend(Options options = {});

  bool initialize(const BasisSolver::SparseColumns& basis_columns,
                  BasisSolver::Index dimension);

  bool ftran(const std::vector<BasisSolver::Real>& rhs,
             std::vector<BasisSolver::Real>& solution);

  bool btran(const std::vector<BasisSolver::Real>& rhs,
             std::vector<BasisSolver::Real>& solution);

  // Returns false when the active backend cannot represent a live pivot
  // update. The implementation then switches to the CPU reference backend
  // before retrying the update, preserving simplex correctness.
  bool update(const std::vector<BasisSolver::Real>& direction,
              BasisSolver::Index leaving_row);

  bool valid() const noexcept;
  const char* backend_name() const noexcept;
  bool gpu_active() const noexcept;
  std::size_t update_count() const noexcept;

private:
  bool activate_cpu();

  Options options_;
  std::unique_ptr<BasisSolver> active_;
  std::unique_ptr<BasisSolver> cpu_;
  bool gpu_active_ = false;
};

}  // namespace indigenous::basis
