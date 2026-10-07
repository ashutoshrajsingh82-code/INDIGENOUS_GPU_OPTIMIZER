#pragma once

#include <cstddef>
#include <vector>

namespace indigenous::pipeline {

// Persistent operation workspace shared by the Phase 5 orchestration layer.
//
// Phase 4.8 remains the owner of CUDA-resident basis matrices and SpSV
// analysis. This class deliberately does not duplicate that device state.
// Instead, it owns reusable solver-operation buffers so FTRAN, BTRAN,
// pricing, and pivot/update paths do not repeatedly grow temporary vectors.
//
// On a CPU-only build these are persistent host buffers. A later CUDA pipeline
// stage can bind the same logical buffers to device allocations/streams.
class UnifiedGpuWorkspace final {
public:
  enum class BufferKind {
    Ftran,
    Btran,
    Pricing,
    Pivot
  };

  struct Report {
    std::size_t allocations = 0;
    std::size_t reuses = 0;
    std::size_t ftran_capacity = 0;
    std::size_t btran_capacity = 0;
    std::size_t pricing_capacity = 0;
    std::size_t pivot_capacity = 0;
    bool persistent = false;
  };

  UnifiedGpuWorkspace() = default;
  ~UnifiedGpuWorkspace() = default;

  UnifiedGpuWorkspace(const UnifiedGpuWorkspace&) = delete;
  UnifiedGpuWorkspace& operator=(const UnifiedGpuWorkspace&) = delete;
  UnifiedGpuWorkspace(UnifiedGpuWorkspace&&) noexcept = default;
  UnifiedGpuWorkspace& operator=(UnifiedGpuWorkspace&&) noexcept = default;

  // Registers the expected basis dimension without allocating every buffer.
  bool initialize(std::size_t dimension);

  // Ensures a reusable buffer has at least element_count entries.
  // Growing a buffer counts as an allocation; fitting an existing capacity
  // counts as a reuse. Existing contents are preserved by std::vector.
  bool ensure(BufferKind kind, std::size_t element_count);

  std::vector<double>& ftran_buffer() noexcept { return ftran_; }
  std::vector<double>& btran_buffer() noexcept { return btran_; }
  std::vector<double>& pricing_buffer() noexcept { return pricing_; }
  std::vector<double>& pivot_buffer() noexcept { return pivot_; }

  const std::vector<double>& ftran_buffer() const noexcept { return ftran_; }
  const std::vector<double>& btran_buffer() const noexcept { return btran_; }
  const std::vector<double>& pricing_buffer() const noexcept { return pricing_; }
  const std::vector<double>& pivot_buffer() const noexcept { return pivot_; }

  std::size_t dimension() const noexcept { return dimension_; }
  bool initialized() const noexcept { return initialized_; }

  std::size_t allocations() const noexcept { return allocations_; }
  std::size_t reuses() const noexcept { return reuses_; }

  bool persistent() const noexcept {
    return initialized_ && allocations_ > 0;
  }

  Report report() const noexcept;

  void release() noexcept;

private:
  std::vector<double>& buffer(BufferKind kind) noexcept;

  std::vector<double> ftran_;
  std::vector<double> btran_;
  std::vector<double> pricing_;
  std::vector<double> pivot_;

  std::size_t dimension_ = 0;
  std::size_t allocations_ = 0;
  std::size_t reuses_ = 0;
  bool initialized_ = false;
};

}  // namespace indigenous::pipeline
