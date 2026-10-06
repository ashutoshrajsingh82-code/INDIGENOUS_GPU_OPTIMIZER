#pragma once

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "solver/common/types.hpp"

namespace indigenous::basis {

class BasisSolver {
public:
  using Real = solver::Real;
  using Index = solver::Index;
  using SparseEntry = std::pair<Index, Real>;
  using SparseColumn = std::vector<SparseEntry>;
  using SparseColumns = std::vector<SparseColumn>;

  virtual ~BasisSolver() = default;

  virtual bool initialize(const SparseColumns& basis_columns,
                          Index dimension,
                          Real pivot_tolerance = 1e-12) = 0;
  virtual bool ftran(const std::vector<Real>& rhs,
                     std::vector<Real>& solution) = 0;
  virtual bool btran(const std::vector<Real>& rhs,
                     std::vector<Real>& solution) = 0;
  virtual bool update(const std::vector<Real>& direction,
                      Index leaving_row,
                      Real pivot_tolerance = 1e-12) = 0;

  virtual bool valid() const noexcept = 0;
  virtual bool available() const noexcept = 0;
  virtual const char* backend_name() const noexcept = 0;
  virtual Index size() const noexcept = 0;
  virtual std::size_t update_count() const noexcept = 0;
};

std::unique_ptr<BasisSolver> make_cpu_basis_solver();

}  // namespace indigenous::basis
