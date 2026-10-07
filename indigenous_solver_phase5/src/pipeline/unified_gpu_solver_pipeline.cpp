#include "pipeline/unified_gpu_solver_pipeline.hpp"

namespace indigenous::pipeline {

UnifiedGpuSolverPipeline::UnifiedGpuSolverPipeline(Options options)
    : options_(options),
      adaptive_selector_({options.prefer_gpu}),
      basis_({options.prefer_gpu, options.pivot_tolerance}) {}

bool UnifiedGpuSolverPipeline::initialize_basis(
    const SparseColumns& columns, Index dimension) {
  basis_decision_ = adaptive_selector_.select(
      AdaptiveBackendSelector::Operation::BasisSolve,
      static_cast<std::size_t>(dimension), [&columns]() {
        std::size_t nnz = 0;
        for (const auto& column : columns) nnz += column.size();
        return nnz;
      }());
  ++adaptive_decisions_;
  basis_initialized_ = basis_.initialize(columns, dimension);
  if (!basis_initialized_) return false;
  if (!workspace_.initialize(static_cast<std::size_t>(dimension))) {
    basis_initialized_ = false;
    return false;
  }
  return true;
}

bool UnifiedGpuSolverPipeline::initialize_pricing(
    const std::vector<std::size_t>& offsets,
    const std::vector<std::size_t>& rows,
    const std::vector<Real>& values,
    const std::vector<Real>& objective) {
  pricing_decision_ = adaptive_selector_.select(
      AdaptiveBackendSelector::Operation::Pricing,
      offsets.empty() ? 0 : (offsets.size() - 1),
      values.size());
  ++adaptive_decisions_;
  pricing_initialized_ = pricing_.initialize(offsets, rows, values, objective);
  if (!pricing_initialized_) return false;
  return workspace_.ensure(UnifiedGpuWorkspace::BufferKind::Pricing,
                           objective.size());
}

bool UnifiedGpuSolverPipeline::ftran(
    const std::vector<Real>& rhs, std::vector<Real>& solution) {
  if (!basis_initialized_ || rhs.size() != workspace_.dimension() ||
      !workspace_.ensure(UnifiedGpuWorkspace::BufferKind::Ftran, rhs.size()))
    return false;

  auto& buffer = workspace_.ftran_buffer();
  if (!basis_.ftran(rhs, buffer)) return false;
  solution = buffer;
  ++ftran_calls_;
  return true;
}

bool UnifiedGpuSolverPipeline::btran(
    const std::vector<Real>& rhs, std::vector<Real>& solution) {
  if (!basis_initialized_ || rhs.size() != workspace_.dimension() ||
      !workspace_.ensure(UnifiedGpuWorkspace::BufferKind::Btran, rhs.size()))
    return false;

  auto& buffer = workspace_.btran_buffer();
  if (!basis_.btran(rhs, buffer)) return false;
  solution = buffer;
  ++btran_calls_;
  return true;
}

bool UnifiedGpuSolverPipeline::price(
    const std::vector<Real>& dual, std::vector<Real>& reduced_costs) {
  if (!pricing_initialized_ || dual.size() != workspace_.dimension()) return false;

  auto& buffer = workspace_.pricing_buffer();
  if (!workspace_.ensure(UnifiedGpuWorkspace::BufferKind::Pricing,
                         pricing_.valid() ? buffer.size() : 0))
    return false;
  if (!pricing_.compute(dual, buffer)) return false;
  reduced_costs = buffer;
  ++pricing_calls_;
  return true;
}

bool UnifiedGpuSolverPipeline::update(
    const std::vector<Real>& direction, Index leaving_row) {
  if (!basis_initialized_ ||
      direction.size() != workspace_.dimension() ||
      !workspace_.ensure(UnifiedGpuWorkspace::BufferKind::Pivot,
                         direction.size()))
    return false;

  auto& buffer = workspace_.pivot_buffer();
  buffer = direction;
  return basis_.update(buffer, leaving_row);
}

bool UnifiedGpuSolverPipeline::btran_and_price(
    const std::vector<Real>& btran_rhs,
    std::vector<Real>& dual,
    std::vector<Real>& reduced_costs) {
  if (!basis_initialized_ || !pricing_initialized_ ||
      btran_rhs.size() != workspace_.dimension())
    return false;

  if (!btran(btran_rhs, workspace_.btran_buffer())) {
    dual.clear();
    reduced_costs.clear();
    return false;
  }

  if (!price(workspace_.btran_buffer(), reduced_costs)) {
    dual.clear();
    reduced_costs.clear();
    return false;
  }

  dual = workspace_.btran_buffer();
  ++coordination_calls_;
  return true;
}

bool UnifiedGpuSolverPipeline::price_and_ftran(
    const std::vector<Real>& dual,
    const std::vector<Real>& entering_column,
    std::vector<Real>& reduced_costs,
    std::vector<Real>& direction) {
  if (!basis_initialized_ || !pricing_initialized_ ||
      dual.size() != workspace_.dimension() ||
      entering_column.size() != workspace_.dimension())
    return false;

  if (!price(dual, reduced_costs)) {
    reduced_costs.clear();
    direction.clear();
    return false;
  }

  if (!ftran(entering_column, direction)) {
    reduced_costs.clear();
    direction.clear();
    return false;
  }

  ++coordination_calls_;
  return true;
}

bool UnifiedGpuSolverPipeline::coordinate_iteration(
    const std::vector<Real>& btran_rhs,
    const std::vector<Real>& entering_column,
    std::vector<Real>& dual,
    std::vector<Real>& reduced_costs,
    std::vector<Real>& direction) {
  if (!basis_initialized_ || !pricing_initialized_ ||
      btran_rhs.size() != workspace_.dimension() ||
      entering_column.size() != workspace_.dimension()) {
    return false;
  }

  if (!btran(btran_rhs, dual)) {
    dual.clear();
    reduced_costs.clear();
    direction.clear();
    return false;
  }

  if (!price(dual, reduced_costs)) {
    dual.clear();
    reduced_costs.clear();
    direction.clear();
    return false;
  }

  if (!ftran(entering_column, direction)) {
    dual.clear();
    reduced_costs.clear();
    direction.clear();
    return false;
  }

  ++coordination_calls_;
  return true;
}

UnifiedGpuSolverPipeline::AsyncIteration
UnifiedGpuSolverPipeline::coordinate_iteration_async(
    const std::vector<Real>& btran_rhs,
    const std::vector<Real>& entering_column) {
  auto result = std::make_shared<AsyncIterationResult>();

  if (!basis_initialized_ || !pricing_initialized_ ||
      btran_rhs.size() != workspace_.dimension() ||
      entering_column.size() != workspace_.dimension()) {
    return {0, result};
  }

  auto btran_copy = btran_rhs;
  auto entering_copy = entering_column;
  const auto task = async_engine_.submit(
      [this, result, btran = std::move(btran_copy),
       entering = std::move(entering_copy)]() mutable {
        std::lock_guard<std::mutex> lock(async_operation_mutex_);
        result->success = coordinate_iteration(
            btran, entering, result->dual, result->reduced_costs,
            result->direction);
        return result->success;
      });

  return {task, result};
}

bool UnifiedGpuSolverPipeline::wait_async(AsyncIteration& operation) {
  if (operation.task == 0 || !operation.result) return false;
  return async_engine_.wait(operation.task) && operation.result->success;
}

bool UnifiedGpuSolverPipeline::async_ready(
    const AsyncIteration& operation) const {
  return operation.task != 0 && async_engine_.ready(operation.task);
}

bool UnifiedGpuSolverPipeline::wait_all_async() {
  return async_engine_.wait_all();
}

bool UnifiedGpuSolverPipeline::initialized() const noexcept {
  return basis_initialized_ && pricing_initialized_;
}

bool UnifiedGpuSolverPipeline::basis_gpu_active() const noexcept {
  return basis_.gpu_active();
}

bool UnifiedGpuSolverPipeline::pricing_gpu_active() const noexcept {
  return indigenous::gpu::available();
}

bool UnifiedGpuSolverPipeline::gpu_active() const noexcept {
  return basis_gpu_active() || pricing_gpu_active();
}

bool UnifiedGpuSolverPipeline::workspace_persistent() const noexcept {
  return workspace_.persistent();
}

const char* UnifiedGpuSolverPipeline::basis_backend_name() const noexcept {
  return basis_.backend_name();
}

const char* UnifiedGpuSolverPipeline::pricing_backend_name() const noexcept {
  return indigenous::gpu::backend_name();
}

const char* UnifiedGpuSolverPipeline::basis_adaptive_backend() const noexcept {
  return basis_decision_.backend;
}

const char* UnifiedGpuSolverPipeline::pricing_adaptive_backend() const noexcept {
  return pricing_decision_.backend;
}

UnifiedGpuSolverPipeline::Report UnifiedGpuSolverPipeline::report() const noexcept {
  const auto workspace_report = workspace_.report();

  Report result;
  result.basis_backend = basis_.backend_name();
  result.pricing_backend = indigenous::gpu::backend_name();
  result.basis_gpu_active = basis_gpu_active();
  result.pricing_gpu_active = pricing_gpu_active();
  result.gpu_active = gpu_active();
  result.initialized = initialized();
  result.workspace_persistent = workspace_report.persistent;
  result.ftran_calls = ftran_calls_;
  result.btran_calls = btran_calls_;
  result.pricing_calls = pricing_calls_;
  result.coordination_calls = coordination_calls_;
  result.update_count = basis_.update_count();
  result.workspace_allocations = workspace_report.allocations;
  result.workspace_reuses = workspace_report.reuses;
  result.adaptive_gpu_eligible =
      basis_decision_.use_gpu || pricing_decision_.use_gpu;
  result.basis_gpu_recommended = basis_decision_.use_gpu;
  result.pricing_gpu_recommended = pricing_decision_.use_gpu;
  result.adaptive_basis_backend = basis_decision_.backend;
  result.adaptive_pricing_backend = pricing_decision_.backend;
  result.adaptive_decisions = adaptive_decisions_;

  const auto async_report = async_engine_.report();
  result.async_submitted = async_report.submitted;
  result.async_completed = async_report.completed;
  result.async_in_flight = async_report.in_flight;
  result.async_available = async_report.asynchronous;
  // Phase 5.5 owns the asynchronous dispatch boundary. CUDA-capable async
  // execution is only reported when the underlying pipeline is actually
  // running a CUDA backend; CPU async must never masquerade as GPU execution.
  result.async_gpu_capable = gpu_active();
  result.async_backend = gpu_active() ? "CUDA-ASYNC" : "CPU-ASYNC";

  return result;
}

}  // namespace indigenous::pipeline
