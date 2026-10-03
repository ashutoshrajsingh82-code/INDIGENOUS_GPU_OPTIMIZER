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
struct SolveResult { SolveStatus status; Real objective_value=0; std::vector<Real> primal,dual; Real primal_residual=0,dual_residual=0; std::size_t iterations=0; std::string message; };
}
