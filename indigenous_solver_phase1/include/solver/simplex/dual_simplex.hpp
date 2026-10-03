#pragma once
#include "solver/model/linear_model.hpp"
namespace solver { class DualSimplexSolver { public: SolveResult solve(const LinearModel&) const; }; }
