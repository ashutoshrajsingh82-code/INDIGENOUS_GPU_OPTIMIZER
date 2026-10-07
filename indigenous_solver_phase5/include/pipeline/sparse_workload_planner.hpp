#pragma once

#include <cstddef>
#include <cstdint>

namespace indigenous::pipeline {

class SparseWorkloadPlanner final {
public:
  enum class Scale {
    Small,
    Medium,
    Large,
    VeryLarge
  };

  struct Options {
    std::size_t memory_budget_bytes = 256ULL * 1024ULL * 1024ULL;
    std::size_t preferred_batch_vectors = 32;
    std::size_t max_batch_vectors = 256;
  };

  struct ModelStats {
    std::size_t rows = 0;
    std::size_t columns = 0;
    std::size_t nonzeros = 0;
  };

  struct Plan {
    Scale scale = Scale::Small;
    bool sparse = true;
    bool large_scale = false;
    std::size_t estimated_csc_bytes = 0;
    std::size_t estimated_vector_bytes = 0;
    std::size_t recommended_batch_vectors = 1;
    std::size_t chunk_columns = 1;
    const char* strategy = "direct sparse CPU";
  };

  explicit SparseWorkloadPlanner(Options options = {}) : options_(options) {}

  Plan plan(ModelStats stats) const noexcept;

  const Options& options() const noexcept { return options_; }

private:
  Options options_;
};

}  // namespace indigenous::pipeline
