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
  using indigenous::basis::validation::btran_residual;

  Solver::SparseColumns columns = {
      {{0, 2.0}, {1, -2.0}, {2, 4.0}},
      {{0, 1.0}, {1, 8.0}, {2, 5.0}},
      {{1, 17.0}, {2, 4.0}}
  };
  constexpr Solver::Index n = 3;
  const std::vector<double> rhs{10.0, 32.0, 46.0};

  auto solver = indigenous::basis::make_cpu_basis_solver();
  if (!solver->initialize(columns, n)) {
    std::cerr << "BTRAN initialization: FAIL\n";
    return 1;
  }

  std::vector<double> y;
  if (!solver->btran(rhs, y)) {
    std::cerr << "BTRAN solve: FAIL\n";
    return 1;
  }

  const auto residual = btran_residual(columns, n, y, rhs);
  if (!residual.finite || residual.infinity_norm > 1e-10) {
    std::cerr << "BTRAN residual: FAIL (" << residual.infinity_norm << ")\n";
    return 1;
  }

  std::cout << "Phase 4.4 BTRAN validation: PASS (residual "
            << residual.infinity_norm << ")\n";
  return 0;
}
