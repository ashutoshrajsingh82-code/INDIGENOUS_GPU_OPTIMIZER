#include "basis/basis_validation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace indigenous::basis::validation {
namespace {

double dot(const std::vector<double>& a, const std::vector<double>& b) {
  if (a.size() != b.size()) return std::numeric_limits<double>::quiet_NaN();
  double value = 0.0;
  for (std::size_t i = 0; i < a.size(); ++i) value += a[i] * b[i];
  return value;
}

bool valid_dimension(BasisSolver::Index dimension,
                     const BasisSolver::SparseColumns& columns) {
  return dimension > 0 &&
         static_cast<std::size_t>(dimension) == columns.size();
}

Residual residual_impl(const BasisSolver::SparseColumns& columns,
                       BasisSolver::Index dimension,
                       const std::vector<double>& solution,
                       const std::vector<double>& rhs,
                       bool transpose) {
  Residual result;
  if (!valid_dimension(dimension, columns) ||
      solution.size() != static_cast<std::size_t>(dimension) ||
      rhs.size() != static_cast<std::size_t>(dimension)) {
    return result;
  }

  double max_error = 0.0;
  for (BasisSolver::Index equation = 0; equation < dimension; ++equation) {
    double value = 0.0;
    if (!transpose) {
      for (BasisSolver::Index column = 0; column < dimension; ++column) {
        for (const auto& [row, coefficient] :
             columns[static_cast<std::size_t>(column)]) {
          if (row == equation) {
            value += coefficient *
                     solution[static_cast<std::size_t>(column)];
            break;
          }
        }
      }
    } else {
      for (const auto& [row, coefficient] :
           columns[static_cast<std::size_t>(equation)]) {
        if (row < dimension)
          value += coefficient * solution[static_cast<std::size_t>(row)];
      }
    }

    const double error =
        std::abs(value - rhs[static_cast<std::size_t>(equation)]);
    if (!std::isfinite(error)) return result;
    max_error = std::max(max_error, error);
  }

  result.infinity_norm = max_error;
  result.finite = true;
  return result;
}

}  // namespace

Residual ftran_residual(const BasisSolver::SparseColumns& basis_columns,
                        BasisSolver::Index dimension,
                        const std::vector<double>& solution,
                        const std::vector<double>& rhs) {
  return residual_impl(basis_columns, dimension, solution, rhs, false);
}

Residual btran_residual(const BasisSolver::SparseColumns& basis_columns,
                        BasisSolver::Index dimension,
                        const std::vector<double>& solution,
                        const std::vector<double>& rhs) {
  return residual_impl(basis_columns, dimension, solution, rhs, true);
}

bool approximately_equal(double a, double b, double absolute_tolerance,
                         double relative_tolerance) {
  if (!std::isfinite(a) || !std::isfinite(b)) return false;
  const double scale = std::max({1.0, std::abs(a), std::abs(b)});
  return std::abs(a - b) <= absolute_tolerance +
         relative_tolerance * scale;
}

bool ftran_btran_duality_check(
    BasisSolver& solver,
    const std::vector<double>& u,
    const std::vector<double>& v,
    double absolute_tolerance,
    double relative_tolerance) {
  std::vector<double> x;
  std::vector<double> y;
  if (!solver.ftran(u, x) || !solver.btran(v, y)) return false;
  const double lhs = dot(x, v);
  const double rhs = dot(u, y);
  return approximately_equal(lhs, rhs, absolute_tolerance,
                             relative_tolerance);
}

}  // namespace indigenous::basis::validation
