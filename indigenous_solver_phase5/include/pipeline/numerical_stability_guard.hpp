#pragma once

#include <cstddef>
#include <cmath>
#include <limits>
#include <vector>

namespace indigenous::pipeline {

class NumericalStabilityGuard final {
public:
  enum class Failure {
    None,
    NonFiniteInput,
    NonFiniteOutput,
    ResidualTooLarge
  };

  struct Options {
    double relative_tolerance = 1e-9;
    double absolute_tolerance = 1e-12;
  };

  struct Result {
    bool valid = true;
    Failure failure = Failure::None;
    double residual = 0.0;
  };

  explicit NumericalStabilityGuard(Options options = {}) : options_(options) {}

  Result validate_input(const std::vector<double>& values) const noexcept;
  Result validate_vector(const std::vector<double>& values) const noexcept;

  Result validate_residual(const std::vector<double>& lhs,
                           const std::vector<double>& rhs,
                           const std::vector<double>& solution) const noexcept;

  static const char* failure_name(Failure failure) noexcept;

  const Options& options() const noexcept { return options_; }

private:
  Options options_;
};

}  // namespace indigenous::pipeline
