#include "pipeline/sparse_workload_planner.hpp"

#include <algorithm>
#include <limits>

namespace indigenous::pipeline {
namespace {
std::size_t sat_add(std::size_t a, std::size_t b) noexcept {
  if (b > std::numeric_limits<std::size_t>::max() - a)
    return std::numeric_limits<std::size_t>::max();
  return a + b;
}

std::size_t sat_mul(std::size_t a, std::size_t b) noexcept {
  if (a != 0 && b > std::numeric_limits<std::size_t>::max() / a)
    return std::numeric_limits<std::size_t>::max();
  return a * b;
}
}  // namespace

SparseWorkloadPlanner::Plan SparseWorkloadPlanner::plan(
    ModelStats stats) const noexcept {
  Plan result;

  const std::size_t matrix_elements = sat_mul(stats.rows, stats.columns);
  result.sparse = matrix_elements == 0 ||
                  stats.nonzeros <= matrix_elements / 20;  // <= 5%

  // CSC uses row indices + values per nonzero and one offset per column.
  result.estimated_csc_bytes =
      sat_add(sat_mul(stats.nonzeros, sizeof(std::size_t) + sizeof(double)),
              sat_mul(stats.columns + 1, sizeof(std::size_t)));

  // Keep room for a few simultaneous double vectors. This is a planning
  // estimate, not an allocation request.
  result.estimated_vector_bytes =
      sat_mul(stats.rows, sizeof(double) * 4);

  const std::size_t max_dimension = std::max(stats.rows, stats.columns);
  if (stats.nonzeros >= 10'000'000 || max_dimension >= 100'000) {
    result.scale = Scale::VeryLarge;
  } else if (stats.nonzeros >= 1'000'000 || max_dimension >= 10'000) {
    result.scale = Scale::Large;
  } else if (stats.nonzeros >= 100'000 || max_dimension >= 2'000) {
    result.scale = Scale::Medium;
  }

  result.large_scale =
      result.scale == Scale::Large || result.scale == Scale::VeryLarge;

  if (!result.large_scale) {
    result.recommended_batch_vectors =
        std::max<std::size_t>(1, std::min(options_.preferred_batch_vectors,
                                          options_.max_batch_vectors));
    result.chunk_columns = std::max<std::size_t>(1, stats.columns);
    result.strategy = result.sparse ? "sparse CPU/GPU adaptive"
                                    : "dense-aware CPU/GPU adaptive";
    return result;
  }

  // Reserve only a fraction of the configured budget for operation vectors;
  // the matrix representation and solver state need the remainder.
  const std::size_t reserved = std::min(
      options_.memory_budget_bytes / 2, result.estimated_csc_bytes);
  const std::size_t vector_budget =
      options_.memory_budget_bytes > reserved
          ? options_.memory_budget_bytes - reserved
          : sizeof(double) * 4;

  const std::size_t bytes_per_batch_vector =
      std::max<std::size_t>(sizeof(double), sat_mul(stats.rows, sizeof(double)));
  const std::size_t capacity =
      vector_budget / bytes_per_batch_vector;

  result.recommended_batch_vectors = std::max<std::size_t>(
      1, std::min({options_.preferred_batch_vectors,
                   options_.max_batch_vectors, capacity}));

  // For very large models, process columns in bounded chunks so callers do
  // not need to materialize all derived work at once.
  const std::size_t nnz_per_column =
      stats.columns == 0 ? stats.nonzeros
                         : (stats.nonzeros + stats.columns - 1) / stats.columns;
  const std::size_t bytes_per_column =
      sat_mul(std::max<std::size_t>(1, nnz_per_column),
              sizeof(std::size_t) + sizeof(double));
  const std::size_t chunk_capacity =
      bytes_per_column == 0 ? 1
                            : std::max<std::size_t>(
                                  1, options_.memory_budget_bytes /
                                         std::max<std::size_t>(1, bytes_per_column));
  result.chunk_columns = std::max<std::size_t>(
      1, std::min(stats.columns == 0 ? 1 : stats.columns, chunk_capacity));

  if (result.scale == Scale::VeryLarge)
    result.strategy = result.sparse ? "chunked sparse CPU/GPU adaptive"
                                    : "chunked dense-aware CPU/GPU adaptive";
  else
    result.strategy = result.sparse ? "batched sparse CPU/GPU adaptive"
                                    : "batched dense-aware CPU/GPU adaptive";

  return result;
}

}  // namespace indigenous::pipeline
