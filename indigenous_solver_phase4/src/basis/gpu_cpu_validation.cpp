#include "basis/gpu_cpu_validation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace indigenous::basis::validation {

BackendComparison compare_vectors(
    const std::vector<double>& cpu,
    const std::vector<double>& gpu,
    double absolute_tolerance,
    double relative_tolerance) {
  BackendComparison result;
  if (cpu.size() != gpu.size() || cpu.empty()) return result;

  double scale = 1.0;
  for (std::size_t i = 0; i < cpu.size(); ++i) {
    if (!std::isfinite(cpu[i]) || !std::isfinite(gpu[i])) return result;
    scale = std::max(scale, std::abs(cpu[i]));
    scale = std::max(scale, std::abs(gpu[i]));
  }

  for (std::size_t i = 0; i < cpu.size(); ++i) {
    const double error = std::abs(cpu[i] - gpu[i]);
    result.max_absolute_error = std::max(result.max_absolute_error, error);
    result.max_relative_error =
        std::max(result.max_relative_error, error /
                 std::max({1.0, std::abs(cpu[i]), std::abs(gpu[i])}));
  }

  result.finite = true;
  result.within_tolerance =
      result.max_absolute_error <=
      absolute_tolerance + relative_tolerance * scale;
  return result;
}

bool residual_within_tolerance(double residual, double scale,
                               double absolute_tolerance,
                               double relative_tolerance) {
  return std::isfinite(residual) && std::isfinite(scale) &&
         residual <= absolute_tolerance +
                     relative_tolerance * std::max(1.0, std::abs(scale));
}

}  // namespace indigenous::basis::validation
