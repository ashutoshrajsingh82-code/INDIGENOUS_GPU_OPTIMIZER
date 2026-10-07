#include "pipeline/unified_gpu_solver_pipeline.hpp"
namespace indigenous::pipeline {
UnifiedGpuSolverPipeline::UnifiedGpuSolverPipeline(Options options)
    : options_(options), basis_({options.prefer_gpu, options.pivot_tolerance}) {}
bool UnifiedGpuSolverPipeline::initialize_basis(const SparseColumns& columns, Index dimension) {
  basis_initialized_ = basis_.initialize(columns, dimension);
  return basis_initialized_;
}
bool UnifiedGpuSolverPipeline::initialize_pricing(
    const std::vector<std::size_t>& offsets, const std::vector<std::size_t>& rows,
    const std::vector<Real>& values, const std::vector<Real>& objective) {
  pricing_initialized_ = pricing_.initialize(offsets, rows, values, objective);
  return pricing_initialized_;
}
bool UnifiedGpuSolverPipeline::ftran(const std::vector<Real>& rhs, std::vector<Real>& solution) {
  if (!basis_initialized_ || !basis_.ftran(rhs, solution)) return false;
  ++ftran_calls_; return true;
}
bool UnifiedGpuSolverPipeline::btran(const std::vector<Real>& rhs, std::vector<Real>& solution) {
  if (!basis_initialized_ || !basis_.btran(rhs, solution)) return false;
  ++btran_calls_; return true;
}
bool UnifiedGpuSolverPipeline::price(const std::vector<Real>& dual, std::vector<Real>& reduced_costs) {
  if (!pricing_initialized_ || !pricing_.compute(dual, reduced_costs)) return false;
  ++pricing_calls_; return true;
}
bool UnifiedGpuSolverPipeline::update(const std::vector<Real>& direction, Index leaving_row) {
  return basis_initialized_ && basis_.update(direction, leaving_row);
}
bool UnifiedGpuSolverPipeline::initialized() const noexcept {
  return basis_initialized_ && pricing_initialized_;
}
bool UnifiedGpuSolverPipeline::gpu_active() const noexcept { return basis_.gpu_active(); }
const char* UnifiedGpuSolverPipeline::basis_backend_name() const noexcept {
  return basis_.backend_name();
}
const char* UnifiedGpuSolverPipeline::pricing_backend_name() const noexcept {
  return indigenous::gpu::backend_name();
}
UnifiedGpuSolverPipeline::Report UnifiedGpuSolverPipeline::report() const noexcept {
  Report result;
  result.basis_backend = basis_.backend_name();
  result.pricing_backend = indigenous::gpu::backend_name();
  result.gpu_active = gpu_active();
  result.initialized = initialized();
  result.ftran_calls = ftran_calls_;
  result.btran_calls = btran_calls_;
  result.pricing_calls = pricing_calls_;
  result.update_count = basis_.update_count();
  return result;
}
}  // namespace indigenous::pipeline
