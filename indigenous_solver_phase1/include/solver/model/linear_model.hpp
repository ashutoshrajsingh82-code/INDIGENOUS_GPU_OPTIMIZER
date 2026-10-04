#pragma once
#include <string>
#include <vector>
#include "solver/common/types.hpp"
#include "solver/common/status.hpp"
#include "solver/sparse/csc_matrix.hpp"
namespace solver {
struct Variable { std::string name; Real lower_bound=0; Real upper_bound=kInfinity; Real objective=0; bool integer=false; };
struct Constraint { std::string name; Real lower_bound=-kInfinity; Real upper_bound=kInfinity; };
struct LinearModel { std::string name="MODEL"; bool minimize=true; std::vector<Variable> variables; std::vector<Constraint> constraints; CscMatrix A;
 bool validate(std::string& error) const; Real objective_value(const std::vector<Real>& x) const; };
struct SimplexStatistics {
  double total_ms=0;
  double lu_factorization_ms=0;
  double ftran_ms=0;
  double btran_ms=0;
  double pricing_ms=0;
  double pivot_ms=0;
  double ratio_test_ms=0;
  double basis_update_ms=0;
  std::size_t lu_factorizations=0;
  std::size_t ftran_solves=0;
  std::size_t btran_solves=0;
  std::size_t pivots=0;
  std::size_t max_lu_nonzeros=0;
};
struct SolveResult {
  SolveStatus status;
  Real objective_value=0;
  std::vector<Real> primal,dual;
  Real primal_residual=0,dual_residual=0;
  std::size_t iterations=0;
  std::string message;
  SimplexStatistics statistics;
};
}
