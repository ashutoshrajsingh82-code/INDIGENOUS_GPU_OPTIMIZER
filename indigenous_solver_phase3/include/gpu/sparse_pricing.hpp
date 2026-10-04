#pragma once

#include <cstddef>
#include <vector>

namespace indigenous::gpu {

// Computes reduced costs r_j = c_j - A_j^T * pi for a sparse CSC matrix.
// column_offsets has size column_count + 1; row_indices and values contain
// the nonzeros of all columns. Returns false on invalid dimensions or backend failure.
bool sparse_reduced_costs(
    const std::vector<std::size_t>& column_offsets,
    const std::vector<std::size_t>& row_indices,
    const std::vector<double>& values,
    const std::vector<double>& objective,
    const std::vector<double>& dual,
    std::vector<double>& reduced_costs);

} // namespace indigenous::gpu
