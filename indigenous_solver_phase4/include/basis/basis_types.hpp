#pragma once

#include <cstddef>

namespace indigenous::basis {

struct BasisTiming {
  double factorization_ms = 0.0;
  double ftran_ms = 0.0;
  double btran_ms = 0.0;
  double update_ms = 0.0;
};

struct BasisStatistics {
  std::size_t factorization_count = 0;
  std::size_t ftran_solves = 0;
  std::size_t btran_solves = 0;
  std::size_t updates = 0;
};

}  // namespace indigenous::basis
