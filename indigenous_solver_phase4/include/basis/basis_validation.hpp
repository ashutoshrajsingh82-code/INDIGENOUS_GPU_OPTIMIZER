#pragma once

#include <vector>

#include "basis/basis_solver.hpp"

namespace indigenous::basis::validation {

struct Residual {
  double infinity_norm = 0.0;
  bool finite = false;
};

Residual ftran_residual(const BasisSolver::SparseColumns& basis_columns,
                        BasisSolver::Index dimension,
                        const std::vector<BasisSolver::Real>& solution,
                        const std::vector<BasisSolver::Real>& rhs);

Residual btran_residual(const BasisSolver::SparseColumns& basis_columns,
                        BasisSolver::Index dimension,
                        const std::vector<BasisSolver::Real>& solution,
                        const std::vector<BasisSolver::Real>& rhs);

bool approximately_equal(double a, double b, double absolute_tolerance,
                         double relative_tolerance);

bool ftran_btran_duality_check(
    BasisSolver& solver,
    const std::vector<BasisSolver::Real>& u,
    const std::vector<BasisSolver::Real>& v,
    double absolute_tolerance = 1e-10,
    double relative_tolerance = 1e-9);

}  // namespace indigenous::basis::validation
