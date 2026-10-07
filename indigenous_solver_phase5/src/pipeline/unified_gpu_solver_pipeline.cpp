#include "pipeline/unified_gpu_solver_pipeline.hpp"
namespace indigenous::pipeline {
UnifiedGpuSolverPipeline::UnifiedGpuSolverPipeline(Options options)
    : options_(options), basis_({options.prefer_gpu, options.pivot_tolerance}) {}
bool UnifiedGpuSolverPipeline::initialize_basis(const SparseColumns& columns, Index dimension) {
  basis_initialized_ = basis_.initialize(columns, dimension);
  if (!basis_initialized_) return false;
  if (!workspace_.initialize(static_cast<std::size_t>(dimension))) {
    basis_initialized_ = false;
    return false;
  }
  return true;
}
bool UnifiedGpuSolverPipeline::initialize_pricing(
    const std::vector<std::size_t>& offsets, const std::vector<std::size_t>& rows,
    const std::vector<Real>& values, const std::vector<Real>& objective) {
  pricing_initialized_ = pricing_.initialize(offsets, rows, values, objective);
  if (!pricing_initialized_) return false;
  return workspace_.ensure(UnifiedGpuWorkspace::BufferKind::Pricing, objective.size());
}
bool UnifiedGpuSolverPipeline::ftran(const std::vector<Real>& rhs, std::vector<Real>& solution) {
  if (!basis_initialized_ || rhs.size() != workspace_.dimension() ||
      !workspace_.ensure(UnifiedGpuWorkspace::BufferKind::Ftran, rhs.size())) return false;
  auto& buffer = workspace_.ftran_buffer();
  if (!basis_.ftran(rhs, buffer)) return false;
  solution = buffer;
  ++ftran_calls_;
  return true;
}
bool UnifiedGpuSolverPipeline::btran(const std::vector<Real>& rhs, std::vector<Real>& solution) {
  if (!basis_initialized_ || rhs.size() != workspace_.dimension() ||
      !workspace_.ensure(UnifiedGpuWorkspace::BufferKind::Btran, rhs.size())) return false;
  auto& buffer = workspace_.btran_buffer();
  if (!basis_.btran(rhs, buffer)) return false;
  solution = buffer;
  ++btran_calls_;
  return true;
}
bool UnifiedGpuSolverPipeline::price(const std::vector<Real>& dual, std::vector<Real>& reduced_costs) {
  if (!pricing_initialized_) return false;
  auto& buffer = workspace_.pricing_buffer();
  if (!pricing_.compute(dual, buffer)) return false;
  reduced_costs = buffer;
  ++pricing_calls_;
  return true;
}
bool UnifiedGpuSolverPipeline::update(const std::vector<Real>& direction, Index leaving_row) {
  if (!basis_initialized_ ||
      !workspace_.ensure(UnifiedGpuWorkspace::BufferKind::Pivot, direction.size())) return false;
  auto& buffer = workspace_.pivot_buffer();
  buffer = direction;
  return basis_.update(buffer, leaving_row);
}
bool UnifiedGpuSolverPipeline::initialized() const noexcept {
  return basis_initialized_ && pricing_initialized_;
}
bool UnifiedGpuSolverPipeline::gpu_active() const noexcept { return basis_.gpu_active(); }
bool UnifiedGpuSolverPipeline::workspace_persistent() const noexcept { return workspace_.persistent(); }
const char* UnifiedGpuSolverPipeline::basis_backend_name() const noexcept { return basis_.backend_name(); }
const char* UnifiedGpuSolverPipeline::pricing_backend_name() const noexcept {
  return indigenous::gpu::backend_name();
}
UnifiedGpuSolverPipeline::Report UnifiedGpuSolverPipeline::report() const noexcept {
  const auto workspace_report = workspace_.report();
  Report result;
  result.basis_backend = basis_.backend_name();
  result.pricing_backend = indigenous::gpu::backend_name();
  result.gpu_active = gpu_active();
  result.initialized = initialized();
  result.workspace_persistent = workspace_report.persistent;
  result.ftran_calls = ftran_calls_;
  result.btran_calls = btran_calls_;
  result.pricing_calls = pricing_calls_;
  result.update_count = basis_.update_count();
  result.workspace_allocations = workspace_report.allocations;
  result.workspace_reuses = workspace_report.reuses;
  return result;
}
}  // namespace indigenous::pipeline
