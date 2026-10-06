#include "basis/basis_solver.hpp"

#include <memory>

#include "solver/linalg/sparse_lu.hpp"

namespace indigenous::basis {

class CpuBasisSolver final : public BasisSolver {
public:
  bool initialize(const SparseColumns& basis_columns,
                  Index dimension,
                  Real pivot_tolerance) override {
    return lu_.factorize_sparse_columns(basis_columns, dimension,
                                        pivot_tolerance);
  }

  bool ftran(const std::vector<Real>& rhs,
             std::vector<Real>& solution) override {
    return lu_.solve(rhs, solution);
  }

  bool btran(const std::vector<Real>& rhs,
             std::vector<Real>& solution) override {
    return lu_.solve_transpose(rhs, solution);
  }

  bool update(const std::vector<Real>& direction,
              Index leaving_row,
              Real pivot_tolerance) override {
    return lu_.update(direction, leaving_row, pivot_tolerance);
  }

  bool valid() const noexcept override {
    return lu_.size() != 0;
  }

  bool available() const noexcept override {
    return true;
  }

  const char* backend_name() const noexcept override {
    return "CPU";
  }

  Index size() const noexcept override {
    return lu_.size();
  }

  std::size_t update_count() const noexcept override {
    return lu_.update_count();
  }

private:
  solver::SparseLU lu_;
};

std::unique_ptr<BasisSolver> make_cpu_basis_solver() {
  return std::make_unique<CpuBasisSolver>();
}

}  // namespace indigenous::basis
