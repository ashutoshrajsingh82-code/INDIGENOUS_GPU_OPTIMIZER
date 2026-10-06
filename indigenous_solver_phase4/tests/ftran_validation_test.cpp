#include "basis/basis_solver.hpp"
#include "basis/basis_validation.hpp"

#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

namespace indigenous::basis {
std::unique_ptr<BasisSolver> make_cpu_basis_solver();
}

int main() {
  using Solver = indigenous::basis::BasisSolver;
  using indigenous::basis::validation::ftran_residual;

  Solver::SparseColumns columns = {
      {{0, 2.0}, {1, -2.0}, {2, 4.0}},
      {{0, 1.0}, {1, 8.0}, {2, 5.0}},
      {{1, 17.0}, {2, 4.0}}
  };
  constexpr Solver::Index n = 3;
  const std::vector<double> rhs{3.0, -10.0, 7.0};

  auto solver = indigenous::basis::make_cpu_basis_solver();
  if (!solver->initialize(columns, n)) {
    std::cerr << "FTRAN initialization: FAIL\n";
    return 1;
  }

  std::vector<double> x;
  if (!solver->ftran(rhs, x)) {
    std::cerr << "FTRAN solve: FAIL\n";
    return 1;
  }

  const auto residual = ftran_residual(columns, n, x, rhs);
  if (!residual.finite || residual.infinity_norm > 1e-10) {
    std::cerr << "FTRAN residual: FAIL (" << residual.infinity_norm << ")\n";
    return 1;
  }

  std::cout << "Phase 4.3 FTRAN validation: PASS (residual "
            << residual.infinity_norm << ")\n";
  return 0;
}
