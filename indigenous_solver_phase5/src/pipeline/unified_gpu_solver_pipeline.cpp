#include "pipeline/unified_gpu_solver_pipeline.hpp"

#include <algorithm>
#include <cmath>

namespace indigenous::pipeline {

UnifiedGpuSolverPipeline::UnifiedGpuSolverPipeline(Options options)
    : options_(options),
      adaptive_selector_({options.prefer_gpu}),
      sparse_planner_({options.memory_budget_bytes,
                       options.preferred_batch_vectors,
                       options.max_batch_vectors}),
      stability_guard_({options.numerical_relative_tolerance,
                        options.numerical_absolute_tolerance}),
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
  sparse_stats_.rows = static_cast<std::size_t>(dimension);
  sparse_stats_.columns = columns.size();
  sparse_stats_.nonzeros = 0;
  for (const auto& column : columns)
    sparse_stats_.nonzeros += column.size();
  sparse_plan_ = sparse_planner_.plan(sparse_stats_);
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
  sparse_stats_.rows = offsets.empty() ? sparse_stats_.rows
                                       : sparse_stats_.rows;
  sparse_stats_.columns = offsets.empty() ? 0 : offsets.size() - 1;
  sparse_stats_.nonzeros = values.size();
  sparse_plan_ = sparse_planner_.plan(sparse_stats_);
  pricing_offsets_ = offsets;
  pricing_rows_ = rows;
  pricing_values_ = values;
  pricing_objective_ = objective;
  pricing_initialized_ = pricing_.initialize(offsets, rows, values, objective);
  if (!pricing_initialized_) return false;
  return workspace_.ensure(UnifiedGpuWorkspace::BufferKind::Pricing,
                           objective.size());
}

bool UnifiedGpuSolverPipeline::ftran(
    const std::vector<Real>& rhs, std::vector<Real>& solution) {
  ScopedPerformanceTimer timer(profiler_, PerformanceProfiler::Stage::Ftran);
  if (!basis_initialized_ || rhs.size() != workspace_.dimension() ||
      !workspace_.ensure(UnifiedGpuWorkspace::BufferKind::Ftran, rhs.size()))
    return false;
  if (!stability_guard_.validate_input(rhs).valid) {
    ++numerical_checks_;
    numerical_stable_ = false;
    ++numerical_failures_;
    last_numerical_failure_ = "NONFINITE_INPUT";
    return false;
  }

  auto& buffer = workspace_.ftran_buffer();
  if (!basis_.ftran(rhs, buffer)) return false;
  const auto check = validate_result(buffer);
  if (!check.valid) {
    if (options_.enable_numerical_fallback && basis_.gpu_active()) {
      if (basis_.recover_cpu() && basis_.ftran(rhs, buffer)) {
        const auto retry = validate_result(buffer);
        if (retry.valid) { ++fallback_count_; solution = buffer; return true; }
      }
    }
    return false;
  }
  solution = buffer;
  ++ftran_calls_;
  return true;
}

bool UnifiedGpuSolverPipeline::btran(
    const std::vector<Real>& rhs, std::vector<Real>& solution) {
  ScopedPerformanceTimer timer(profiler_, PerformanceProfiler::Stage::Btran);
  if (!basis_initialized_ || rhs.size() != workspace_.dimension() ||
      !workspace_.ensure(UnifiedGpuWorkspace::BufferKind::Btran, rhs.size()))
    return false;
  if (!stability_guard_.validate_input(rhs).valid) {
    ++numerical_checks_;
    numerical_stable_ = false;
    ++numerical_failures_;
    last_numerical_failure_ = "NONFINITE_INPUT";
    return false;
  }

  auto& buffer = workspace_.btran_buffer();
  if (!basis_.btran(rhs, buffer)) return false;
  const auto check = validate_result(buffer);
  if (!check.valid) {
    if (options_.enable_numerical_fallback && basis_.gpu_active()) {
      if (basis_.recover_cpu() && basis_.btran(rhs, buffer)) {
        const auto retry = validate_result(buffer);
        if (retry.valid) { ++fallback_count_; solution = buffer; return true; }
      }
    }
    return false;
  }
  solution = buffer;
  ++btran_calls_;
  return true;
}

bool UnifiedGpuSolverPipeline::price(
    const std::vector<Real>& dual, std::vector<Real>& reduced_costs) {
  ScopedPerformanceTimer timer(profiler_, PerformanceProfiler::Stage::Pricing);
  if (!pricing_initialized_ || dual.size() != workspace_.dimension()) return false;
  if (!stability_guard_.validate_input(dual).valid) {
    ++numerical_checks_;
    numerical_stable_ = false;
    ++numerical_failures_;
    last_numerical_failure_ = "NONFINITE_INPUT";
    return false;
  }

  auto& buffer = workspace_.pricing_buffer();
  if (!workspace_.ensure(UnifiedGpuWorkspace::BufferKind::Pricing,
                         pricing_.valid() ? buffer.size() : 0))
    return false;
  if (!pricing_.compute(dual, buffer)) return false;
  const auto check = validate_result(buffer);
  if (!check.valid) {
    if (options_.enable_numerical_fallback &&
        !pricing_offsets_.empty() && !pricing_objective_.empty()) {
      reduced_costs = pricing_objective_;
      for (std::size_t j = 0; j < pricing_objective_.size(); ++j) {
        for (std::size_t p = pricing_offsets_[j]; p < pricing_offsets_[j + 1]; ++p)
          reduced_costs[j] -= pricing_values_[p] * dual[pricing_rows_[p]];
      }
      const auto retry = validate_result(reduced_costs);
      if (retry.valid) { ++fallback_count_; return true; }
    }
    return false;
  }
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
  ScopedPerformanceTimer timer(profiler_, PerformanceProfiler::Stage::Coordination);
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

bool UnifiedGpuSolverPipeline::batch_ftran(
    const VectorBatch& rhs_batch,
    VectorBatch& solution_batch) {
  std::lock_guard<std::mutex> lock(async_operation_mutex_);
  if (!basis_initialized_) return false;

  for (const auto& rhs : rhs_batch) {
    if (rhs.size() != workspace_.dimension()) return false;
  }

  solution_batch.clear();
  solution_batch.reserve(rhs_batch.size());
  for (const auto& rhs : rhs_batch) {
    std::vector<Real> solution;
    if (!ftran(rhs, solution)) {
      solution_batch.clear();
      return false;
    }
    solution_batch.push_back(std::move(solution));
  }

  ++batch_calls_;
  batch_vectors_processed_ += rhs_batch.size();
  return true;
}

bool UnifiedGpuSolverPipeline::batch_btran(
    const VectorBatch& rhs_batch,
    VectorBatch& solution_batch) {
  std::lock_guard<std::mutex> lock(async_operation_mutex_);
  if (!basis_initialized_) return false;

  for (const auto& rhs : rhs_batch) {
    if (rhs.size() != workspace_.dimension()) return false;
  }

  solution_batch.clear();
  solution_batch.reserve(rhs_batch.size());
  for (const auto& rhs : rhs_batch) {
    std::vector<Real> solution;
    if (!btran(rhs, solution)) {
      solution_batch.clear();
      return false;
    }
    solution_batch.push_back(std::move(solution));
  }

  ++batch_calls_;
  batch_vectors_processed_ += rhs_batch.size();
  return true;
}

bool UnifiedGpuSolverPipeline::batch_price(
    const VectorBatch& dual_batch,
    VectorBatch& reduced_cost_batch) {
  std::lock_guard<std::mutex> lock(async_operation_mutex_);
  if (!pricing_initialized_) return false;

  for (const auto& dual : dual_batch) {
    if (dual.size() != workspace_.dimension()) return false;
  }

  reduced_cost_batch.clear();
  reduced_cost_batch.reserve(dual_batch.size());
  for (const auto& dual : dual_batch) {
    std::vector<Real> reduced_costs;
    if (!price(dual, reduced_costs)) {
      reduced_cost_batch.clear();
      return false;
    }
    reduced_cost_batch.push_back(std::move(reduced_costs));
  }

  ++batch_calls_;
  batch_vectors_processed_ += dual_batch.size();
  return true;
}

bool UnifiedGpuSolverPipeline::batch_coordinate_iteration(
    const VectorBatch& btran_rhs_batch,
    const VectorBatch& entering_column_batch,
    BatchIterationResult& result) {
  ScopedPerformanceTimer timer(profiler_, PerformanceProfiler::Stage::Batch);
  std::lock_guard<std::mutex> lock(async_operation_mutex_);
  if (!basis_initialized_ || !pricing_initialized_ ||
      btran_rhs_batch.size() != entering_column_batch.size()) {
    return false;
  }

  for (const auto& rhs : btran_rhs_batch) {
    if (rhs.size() != workspace_.dimension()) return false;
  }
  for (const auto& entering : entering_column_batch) {
    if (entering.size() != workspace_.dimension()) return false;
  }

  result = {};
  result.dual.reserve(btran_rhs_batch.size());
  result.reduced_costs.reserve(btran_rhs_batch.size());
  result.direction.reserve(btran_rhs_batch.size());

  for (std::size_t i = 0; i < btran_rhs_batch.size(); ++i) {
    std::vector<Real> dual;
    std::vector<Real> reduced_costs;
    std::vector<Real> direction;
    if (!coordinate_iteration(btran_rhs_batch[i], entering_column_batch[i],
                               dual, reduced_costs, direction)) {
      result = {};
      return false;
    }
    result.dual.push_back(std::move(dual));
    result.reduced_costs.push_back(std::move(reduced_costs));
    result.direction.push_back(std::move(direction));
  }

  result.success = true;
  ++batch_calls_;
  batch_vectors_processed_ += btran_rhs_batch.size();
  return true;
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

SparseWorkloadPlanner::Plan UnifiedGpuSolverPipeline::large_scale_plan() const noexcept {
  return sparse_plan_;
}

NumericalStabilityGuard::Result UnifiedGpuSolverPipeline::validate_result(
    const std::vector<Real>& values) noexcept {
  const auto result = stability_guard_.validate_vector(values);
  ++numerical_checks_;
  if (!result.valid) {
    numerical_stable_ = false;
    ++numerical_failures_;
    last_numerical_failure_ =
        NumericalStabilityGuard::failure_name(result.failure);
  }
  maximum_residual_ = std::max(maximum_residual_, result.residual);
  return result;
}

bool UnifiedGpuSolverPipeline::numerical_stable() const noexcept {
  return numerical_stable_;
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
  result.batch_calls = batch_calls_;
  result.batch_vectors_processed = batch_vectors_processed_;
  result.batch_available = true;
  // Batch dispatch is backend-neutral. CPU-BATCH means the logical batch API
  // is active while the underlying operations execute through CPU fallbacks.
  result.batch_backend = gpu_active() ? "CUDA-BATCH" : "CPU-BATCH";
  result.sparse_rows = sparse_stats_.rows;
  result.sparse_columns = sparse_stats_.columns;
  result.sparse_nonzeros = sparse_stats_.nonzeros;
  switch (sparse_plan_.scale) {
    case SparseWorkloadPlanner::Scale::VeryLarge:
      result.sparse_scale = "VERY_LARGE";
      break;
    case SparseWorkloadPlanner::Scale::Large:
      result.sparse_scale = "LARGE";
      break;
    case SparseWorkloadPlanner::Scale::Medium:
      result.sparse_scale = "MEDIUM";
      break;
    default:
      result.sparse_scale = "SMALL";
      break;
  }
  result.sparse_large_scale = sparse_plan_.large_scale;
  result.estimated_csc_bytes = sparse_plan_.estimated_csc_bytes;
  result.recommended_batch_vectors = sparse_plan_.recommended_batch_vectors;
  result.chunk_columns = sparse_plan_.chunk_columns;
  result.sparse_strategy = sparse_plan_.strategy;
  result.numerical_stable = numerical_stable_;
  result.fallback_active = cpu_fallback_required_ || fallback_count_ > 0;
  result.numerical_checks = numerical_checks_;
  result.numerical_failures = numerical_failures_;
  result.fallback_count = fallback_count_;
  result.maximum_residual = maximum_residual_;
  result.last_numerical_failure = last_numerical_failure_;
  result.performance = profiler_.report();
  profiler_.set_backend(result.gpu_active ? "CUDA" : "CPU");
  result.performance = profiler_.report();

  return result;
}

}  // namespace indigenous::pipeline
