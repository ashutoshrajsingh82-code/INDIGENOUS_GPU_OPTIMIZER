#include "basis/basis_solver.hpp"
#include "basis/basis_validation.hpp"

#include <iostream>
#include <memory>
#include <vector>

namespace indigenous::basis {
std::unique_ptr<BasisSolver> make_cpu_basis_solver();
}

int main() {
  using Solver = indigenous::basis::BasisSolver;

  Solver::SparseColumns columns = {
      {{0, 2.0}, {1, -2.0}, {2, 4.0}},
      {{0, 1.0}, {1, 8.0}, {2, 5.0}},
      {{1, 17.0}, {2, 4.0}}
  };

  auto solver = indigenous::basis::make_cpu_basis_solver();
  if (!solver->initialize(columns, 3)) {
    std::cerr << "Basis solver initialization: FAIL\n";
    return 1;
  }
  if (!solver->available() || std::string(solver->backend_name()) != "CPU") {
    std::cerr << "CPU backend reporting: FAIL\n";
    return 1;
  }

  std::vector<double> u{1.0, 2.0, 3.0};
  std::vector<double> v{4.0, -2.0, 5.0};
  if (!indigenous::basis::validation::ftran_btran_duality_check(
          *solver, u, v)) {
    std::cerr << "FTRAN/BTRAN duality: FAIL\n";
    return 1;
  }

  std::cout << "Phase 4.1/4.2 basis abstraction + CPU backend: PASS\n";
  return 0;
}
