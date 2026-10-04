#include "gpu/sparse_pricing.hpp"

#include <cmath>
#include <iostream>
#include <vector>

int main() {
  // CSC matrix:
  // [ 1  0  2 ]
  // [ 3 -1  0 ]
  const std::vector<std::size_t> offsets{0, 2, 3, 4};
  const std::vector<std::size_t> rows{0, 1, 1, 0};
  const std::vector<double> values{1.0, 3.0, -1.0, 2.0};
  const std::vector<double> objective{5.0, 4.0, 7.0};
  const std::vector<double> dual{0.5, -2.0};

  std::vector<double> reduced;
  if(!indigenous::gpu::sparse_reduced_costs(
         offsets, rows, values, objective, dual, reduced)) {
    std::cerr << "Sparse pricing operation failed\n";
    return 1;
  }

  const std::vector<double> expected{10.5, 2.0, 6.0};
  if(reduced.size() != expected.size()) {
    std::cerr << "Sparse pricing size mismatch\n";
    return 1;
  }
  for(std::size_t j = 0; j < expected.size(); ++j) {
    if(std::abs(reduced[j] - expected[j]) > 1e-12) {
      std::cerr << "Sparse pricing mismatch at column " << j << "\n";
      return 1;
    }
  }

  std::cout << "Sparse reduced-cost pricing test: PASS\n";
  return 0;
}
