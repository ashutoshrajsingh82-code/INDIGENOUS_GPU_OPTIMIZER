#include "gpu/sparse_pricing.hpp"

namespace indigenous::gpu {

bool sparse_reduced_costs(
    const std::vector<std::size_t>& column_offsets,
    const std::vector<std::size_t>& row_indices,
    const std::vector<double>& values,
    const std::vector<double>& objective,
    const std::vector<double>& dual,
    std::vector<double>& reduced_costs) {
  if(column_offsets.empty() || column_offsets.back() != values.size() ||
     row_indices.size() != values.size() ||
     objective.size() + 1 != column_offsets.size()) {
    return false;
  }

  reduced_costs.assign(objective.size(), 0.0);
  for(std::size_t j = 0; j < objective.size(); ++j) {
    double rc = objective[j];
    for(std::size_t p = column_offsets[j]; p < column_offsets[j + 1]; ++p) {
      if(row_indices[p] >= dual.size()) return false;
      rc -= values[p] * dual[row_indices[p]];
    }
    reduced_costs[j] = rc;
  }
  return true;
}

} // namespace indigenous::gpu
