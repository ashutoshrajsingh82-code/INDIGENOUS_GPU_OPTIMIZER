#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "basis/simplex_basis_backend.hpp"

int main() {
  using indigenous::basis::BasisSolver;
  using indigenous::basis::SimplexBasisBackend;

  // B = [ [2, 1],
  //       [0, 1] ]
  // FTRAN([5, 2]) = [2, 2].
  BasisSolver::SparseColumns columns(2);
  columns[0] = {{0, 2.0}};
  columns[1] = {{0, 1.0}, {1, 1.0}};

  SimplexBasisBackend backend;
  assert(backend.initialize(columns, 2));
  assert(backend.valid());
  assert(std::string(backend.backend_name()) == "CPU");
  assert(!backend.gpu_active());

  std::vector<double> ftran;
  assert(backend.ftran({5.0, 2.0}, ftran));
  assert(ftran.size() == 2);
  assert(std::abs(ftran[0] - 2.0) < 1e-12);
  assert(std::abs(ftran[1] - 2.0) < 1e-12);

  // B^T*x = [5, 2] => x = [2.5, -0.5].
  std::vector<double> btran;
  assert(backend.btran({5.0, 2.0}, btran));
  assert(btran.size() == 2);
  assert(std::abs(btran[0] - 2.5) < 1e-12);
  assert(std::abs(btran[1] + 0.5) < 1e-12);

  // Direction for replacing row 0 with [1, 0]^T:
  // B^{-1} a_enter = [0.5, 0]. The CPU reference accepts the eta update.
  assert(backend.update({0.5, 0.0}, 0));
  assert(backend.update_count() == 1);

  std::cout << "Phase 4.9 simplex basis backend integration: PASS\n";
  return 0;
}
