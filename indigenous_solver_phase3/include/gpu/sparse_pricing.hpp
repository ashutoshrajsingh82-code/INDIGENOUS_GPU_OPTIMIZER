#pragma once

#include <cstddef>
#include <vector>

namespace indigenous::gpu {

// Computes reduced costs r_j = c_j - A_j^T * pi for a sparse CSC matrix.
// This one-shot API is retained for simple callers and tests.
bool sparse_reduced_costs(
    const std::vector<std::size_t>& column_offsets,
    const std::vector<std::size_t>& row_indices,
    const std::vector<double>& values,
    const std::vector<double>& objective,
    const std::vector<double>& dual,
    std::vector<double>& reduced_costs);

// Reusable pricing workspace. Matrix structure and objective are initialized
// once; each compute() call only supplies the current dual vector. CUDA builds
// keep the immutable CSC/objective data resident on the device.
class SparsePricingWorkspace {
public:
  SparsePricingWorkspace() = default;
  ~SparsePricingWorkspace();

  SparsePricingWorkspace(const SparsePricingWorkspace&) = delete;
  SparsePricingWorkspace& operator=(const SparsePricingWorkspace&) = delete;
  SparsePricingWorkspace(SparsePricingWorkspace&&) noexcept;
  SparsePricingWorkspace& operator=(SparsePricingWorkspace&&) noexcept;

  bool initialize(
      const std::vector<std::size_t>& column_offsets,
      const std::vector<std::size_t>& row_indices,
      const std::vector<double>& values,
      const std::vector<double>& objective);

  bool compute(
      const std::vector<double>& dual,
      std::vector<double>& reduced_costs);

  bool valid() const noexcept;

private:
  struct Impl;
  Impl* impl_=nullptr;
};

} // namespace indigenous::gpu
