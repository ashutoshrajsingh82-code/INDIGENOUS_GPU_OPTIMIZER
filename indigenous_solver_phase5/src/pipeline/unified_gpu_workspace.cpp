#include "pipeline/unified_gpu_workspace.hpp"

namespace indigenous::pipeline {

bool UnifiedGpuWorkspace::initialize(std::size_t dimension) {
  if (dimension == 0) return false;
  dimension_ = dimension;
  initialized_ = true;
  return true;
}

std::vector<double>& UnifiedGpuWorkspace::buffer(BufferKind kind) noexcept {
  switch (kind) {
    case BufferKind::Ftran: return ftran_;
    case BufferKind::Btran: return btran_;
    case BufferKind::Pricing: return pricing_;
    case BufferKind::Pivot: return pivot_;
  }
  return ftran_;
}

bool UnifiedGpuWorkspace::ensure(BufferKind kind, std::size_t element_count) {
  if (!initialized_) return false;

  auto& target = buffer(kind);
  if (target.capacity() >= element_count) {
    target.resize(element_count);
    ++reuses_;
    return true;
  }

  target.resize(element_count);
  ++allocations_;
  return true;
}

UnifiedGpuWorkspace::Report UnifiedGpuWorkspace::report() const noexcept {
  Report result;
  result.allocations = allocations_;
  result.reuses = reuses_;
  result.ftran_capacity = ftran_.capacity();
  result.btran_capacity = btran_.capacity();
  result.pricing_capacity = pricing_.capacity();
  result.pivot_capacity = pivot_.capacity();
  result.persistent = persistent();
  return result;
}

void UnifiedGpuWorkspace::release() noexcept {
  std::vector<double>().swap(ftran_);
  std::vector<double>().swap(btran_);
  std::vector<double>().swap(pricing_);
  std::vector<double>().swap(pivot_);
  dimension_ = 0;
  allocations_ = 0;
  reuses_ = 0;
  initialized_ = false;
}

}  // namespace indigenous::pipeline
