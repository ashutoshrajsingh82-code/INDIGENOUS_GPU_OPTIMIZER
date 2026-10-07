#include <iostream>
#include <string>

#include "pipeline/adaptive_backend_selector.hpp"

namespace {
bool check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << "\n";
    return false;
  }
  return true;
}
}

int main() {
  using indigenous::pipeline::AdaptiveBackendSelector;

  AdaptiveBackendSelector selector;

  const auto no_gpu = selector.select(
      AdaptiveBackendSelector::Operation::Pricing, 4096, 100000, false);
  if (!check(!no_gpu.use_gpu, "CPU when CUDA device is unavailable")) return 1;
  if (!check(std::string(no_gpu.backend) == "CPU", "CPU backend name")) return 1;

  const auto small_gpu = selector.select(
      AdaptiveBackendSelector::Operation::Pricing, 64, 10000, true);
  if (!check(!small_gpu.use_gpu, "CPU for small GPU workload")) return 1;

  const auto sparse_gpu = selector.select(
      AdaptiveBackendSelector::Operation::Pricing, 4096, 1024, true);
  if (!check(!sparse_gpu.use_gpu, "CPU for low-nnz workload")) return 1;

  const auto large_pricing = selector.select(
      AdaptiveBackendSelector::Operation::Pricing, 4096, 2000000, true);
  if (!check(large_pricing.use_gpu, "GPU for large pricing workload")) return 1;
  if (!check(std::string(large_pricing.backend) == "GPU",
             "GPU pricing backend name")) return 1;

  const auto large_basis = selector.select(
      AdaptiveBackendSelector::Operation::BasisSolve, 4096, 2000000, true);
  if (!check(large_basis.use_gpu, "GPU for large basis workload")) return 1;

  AdaptiveBackendSelector::Options cpu_options;
  cpu_options.prefer_gpu = false;
  AdaptiveBackendSelector cpu_only(cpu_options);
  const auto disabled = cpu_only.select(
      AdaptiveBackendSelector::Operation::BasisSolve, 4096, 2000000, true);
  if (!check(!disabled.use_gpu, "CPU when GPU preference is disabled")) return 1;

  std::cout << "Phase 5.4 adaptive CPU/GPU workload selection: PASS\n";
  std::cout << "No-GPU: CPU | Small: CPU | Large pricing: GPU | Large basis: GPU\n";
  return 0;
}
