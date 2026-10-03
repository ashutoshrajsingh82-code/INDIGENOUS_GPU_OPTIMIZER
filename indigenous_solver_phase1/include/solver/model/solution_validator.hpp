#pragma once
#include <string>
#include "solver/model/linear_model.hpp"
namespace solver {
struct ValidationReport { bool valid=false; Real max_primal_violation=0; Real recomputed_objective=0; Real objective_difference=0; std::string message; };
ValidationReport validate_solution(const LinearModel& model,const std::vector<Real>& x,Real reported_objective=0);
}
