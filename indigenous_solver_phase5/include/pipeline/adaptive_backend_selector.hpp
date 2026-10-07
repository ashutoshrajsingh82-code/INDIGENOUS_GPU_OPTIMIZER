#pragma once

#include <cstddef>

namespace indigenous::pipeline {

class AdaptiveBackendSelector final {
public:
  enum class Operation {
    BasisSolve,
    Pricing
  };

  struct Options {
    bool prefer_gpu = true;
    std::size_t minimum_dimension = 256;
    std::size_t minimum_nonzeros = 4096;
    std::size_t minimum_work = 1'000'000;
  };

  struct Decision {
    bool use_gpu = false;
    const char* backend = "CPU";
    const char* reason = "CPU fallback";
  };

  explicit AdaptiveBackendSelector(Options options = {}) : options_(options) {}

  Decision select(Operation operation,
                  std::size_t dimension,
                  std::size_t nonzeros) const noexcept;

  Decision select(Operation operation,
                  std::size_t dimension,
                  std::size_t nonzeros,
                  bool gpu_available) const noexcept;

  const Options& options() const noexcept { return options_; }

private:
  Options options_;
};

}  // namespace indigenous::pipeline
