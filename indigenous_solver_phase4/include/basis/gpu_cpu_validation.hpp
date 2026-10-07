#pragma once

#include <cstddef>
#include <vector>

#include "basis/basis_solver.hpp"

namespace indigenous::basis::validation {

struct BackendComparison {
  double max_absolute_error = 0.0;
  double max_relative_error = 0.0;
  double cpu_residual = 0.0;
  double gpu_residual = 0.0;
  bool finite = false;
  bool within_tolerance = false;
};

struct NumericalValidationReport {
  BackendComparison ftran;
  BackendComparison btran;
  double duality_cpu = 0.0;
  double duality_gpu = 0.0;
  bool duality_within_tolerance = false;
  std::size_t ftran_repeats = 0;
  std::size_t btran_repeats = 0;
  bool repeated_calls_stable = false;
  bool passed = false;
};

BackendComparison compare_vectors(
    const std::vector<BasisSolver::Real>& cpu,
    const std::vector<BasisSolver::Real>& gpu,
    double absolute_tolerance = 1e-10,
    double relative_tolerance = 1e-9);

bool residual_within_tolerance(double residual, double scale,
                               double absolute_tolerance = 1e-10,
                               double relative_tolerance = 1e-9);

}  // namespace indigenous::basis::validation
