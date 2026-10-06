#pragma once
#include <cmath>
#include <utility>
#include <vector>
#include "solver/common/types.hpp"

namespace indigenous::basis {

struct BasisFactorization {
  using Index = solver::Index;
  using Real = solver::Real;
  using SparseEntry = std::pair<Index, Real>;
  using SparseRow = std::vector<SparseEntry>;
  using SparseRows = std::vector<SparseRow>;

  SparseRows lower;
  SparseRows upper;
  std::vector<Real> diagonal;
  std::vector<Index> permutation;

  Index size() const noexcept {
    return static_cast<Index>(diagonal.size());
  }

  bool structurally_valid() const noexcept {
    const Index n = size();
    if (n <= 0 || static_cast<std::size_t>(n) != lower.size() ||
        static_cast<std::size_t>(n) != upper.size() ||
        static_cast<std::size_t>(n) != permutation.size()) return false;

    std::vector<bool> seen(static_cast<std::size_t>(n), false);
    for (Index i = 0; i < n; ++i) {
      const auto pi = permutation[static_cast<std::size_t>(i)];
      if (pi < 0 || pi >= n || seen[static_cast<std::size_t>(pi)])
        return false;
      seen[static_cast<std::size_t>(pi)] = true;

      for (const auto& [column, value] : lower[static_cast<std::size_t>(i)])
        if (column < 0 || column >= i || !std::isfinite(value)) return false;

      for (const auto& [column, value] : upper[static_cast<std::size_t>(i)])
        if (column <= i || column >= n || !std::isfinite(value)) return false;

      if (!std::isfinite(diagonal[static_cast<std::size_t>(i)])) return false;
    }
    return true;
  }
};

}  // namespace indigenous::basis
