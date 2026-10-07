#include "pipeline/numerical_stability_guard.hpp"

#include <algorithm>

namespace indigenous::pipeline {

NumericalStabilityGuard::Result
NumericalStabilityGuard::validate_vector(
    const std::vector<double>& values) const noexcept {
  Result result;
  double scale = 1.0;
  for (double value : values) {
    if (!std::isfinite(value)) {
      result.valid = false;
      result.failure = Failure::NonFiniteOutput;
      result.residual = std::numeric_limits<double>::infinity();
      return result;
    }
    scale = std::max(scale, std::abs(value));
  }
  result.residual = scale;
  return result;
}

NumericalStabilityGuard::Result
NumericalStabilityGuard::validate_residual(
    const std::vector<double>& lhs,
    const std::vector<double>& rhs,
    const std::vector<double>& solution) const noexcept {
  Result result;
  if (lhs.size() != rhs.size() || lhs.size() != solution.size()) {
    result.valid = false;
    result.failure = Failure::ResidualTooLarge;
    result.residual = std::numeric_limits<double>::infinity();
    return result;
  }

  double max_error = 0.0;
  double scale = 1.0;
  for (std::size_t i = 0; i < lhs.size(); ++i) {
    if (!std::isfinite(lhs[i]) || !std::isfinite(rhs[i]) ||
        !std::isfinite(solution[i])) {
      result.valid = false;
      result.failure = Failure::NonFiniteInput;
      result.residual = std::numeric_limits<double>::infinity();
      return result;
    }
    max_error = std::max(max_error, std::abs(lhs[i] - rhs[i]));
    scale = std::max(scale, std::abs(rhs[i]));
  }

  result.residual = max_error;
  const double tolerance =
      options_.absolute_tolerance + options_.relative_tolerance * scale;
  if (max_error > tolerance) {
    result.valid = false;
    result.failure = Failure::ResidualTooLarge;
  }
  return result;
}

const char* NumericalStabilityGuard::failure_name(Failure failure) noexcept {
  switch (failure) {
    case Failure::NonFiniteInput: return "NONFINITE_INPUT";
    case Failure::NonFiniteOutput: return "NONFINITE_OUTPUT";
    case Failure::ResidualTooLarge: return "RESIDUAL_TOO_LARGE";
    default: return "NONE";
  }
}

}  // namespace indigenous::pipeline
