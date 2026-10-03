#include "solver/simplex/dual_simplex.hpp"
namespace solver { SolveResult DualSimplexSolver::solve(const LinearModel&) const { return {SolveStatus::UnsupportedModel,0,{}, {},0,0,0,"Dual simplex foundation reserved for Phase 2; robust implementation not yet enabled."}; } }
