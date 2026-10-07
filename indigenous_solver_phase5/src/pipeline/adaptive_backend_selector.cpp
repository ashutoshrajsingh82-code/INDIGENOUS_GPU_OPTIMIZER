#include "pipeline/adaptive_backend_selector.hpp"

#include "gpu/vector_ops.hpp"

namespace indigenous::pipeline {

AdaptiveBackendSelector::Decision AdaptiveBackendSelector::select(
    Operation operation, std::size_t dimension,
    std::size_t nonzeros) const noexcept {
  return select(operation, dimension, nonzeros, indigenous::gpu::available());
}

AdaptiveBackendSelector::Decision AdaptiveBackendSelector::select(
    Operation operation, std::size_t dimension,
    std::size_t nonzeros, bool gpu_available) const noexcept {
  if (!gpu_available)
    return {false, "CPU", "CUDA device unavailable"};

  if (!options_.prefer_gpu)
    return {false, "CPU", "GPU preference disabled"};

  if (dimension < options_.minimum_dimension)
    return {false, "CPU", "workload dimension below GPU threshold"};

  if (nonzeros < options_.minimum_nonzeros)
    return {false, "CPU", "sparse workload below GPU threshold"};

  const std::size_t work =
      operation == Operation::Pricing
          ? nonzeros
          : nonzeros + dimension;

  if (work < options_.minimum_work)
    return {false, "CPU", "estimated workload below GPU threshold"};

  if (operation == Operation::Pricing)
    return {true, "GPU", "pricing workload exceeds GPU threshold"};

  return {true, "GPU", "basis solve workload exceeds GPU threshold"};
}

}  // namespace indigenous::pipeline
