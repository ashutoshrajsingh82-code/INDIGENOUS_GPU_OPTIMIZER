#include "basis/basis_factorization.hpp"

#include <iostream>

int main() {
  indigenous::basis::BasisFactorization factorization;
  factorization.lower = {
      {},
      {{0, 2.0}},
      {{0, -1.0}, {1, 3.0}}
  };
  factorization.upper = {
      {{1, 1.0}},
      {{2, 4.0}},
      {}
  };
  factorization.diagonal = {2.0, 3.0, 5.0};
  factorization.permutation = {0, 2, 1};

  if (!factorization.structurally_valid()) {
    std::cerr << "Basis factorization representation: FAIL\n";
    return 1;
  }

  std::cout << "Phase 4.5 factorization representation: PASS\n";
  return 0;
}
