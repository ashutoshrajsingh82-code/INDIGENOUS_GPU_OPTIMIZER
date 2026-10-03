#pragma once
#include "solver/model/linear_model.hpp"
namespace solver { class PrimalSimplexSolver { public: SolveResult solve(const LinearModel& model,std::size_t max_iterations=10000) const; }; }
