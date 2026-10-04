#pragma once
#include "solver/model/linear_model.hpp"
namespace solver {
struct RevisedSimplexOptions {
  std::size_t max_iterations=100000;
  Real primal_tolerance=1e-8;
  Real dual_tolerance=1e-8;
  Real pivot_tolerance=1e-10;
  std::size_t refactor_frequency=50;
  bool use_devex=true;
};
class RevisedSimplexSolver {
public:
  explicit RevisedSimplexSolver(RevisedSimplexOptions options={}) : options_(options) {}
  SolveResult solve(const LinearModel& model) const;
private:
  RevisedSimplexOptions options_;
};
}
