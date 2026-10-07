#pragma once
#include <cstddef>
#include <memory>
#include <mutex>
#include <vector>
#include "basis/simplex_basis_backend.hpp"
#include "gpu/sparse_pricing.hpp"
#include "pipeline/unified_gpu_workspace.hpp"
#include "pipeline/adaptive_backend_selector.hpp"
#include "pipeline/async_execution_engine.hpp"

namespace indigenous::pipeline {

class UnifiedGpuSolverPipeline final {
public:
  using Real = basis::BasisSolver::Real;
  using Index = basis::BasisSolver::Index;
  using SparseColumns = basis::BasisSolver::SparseColumns;

  struct Options {
    bool prefer_gpu = true;
    Real pivot_tolerance = 1e-12;
  };

  struct Report {
    const char* basis_backend = "NONE";
    const char* pricing_backend = "UNKNOWN";
    bool gpu_active = false;
    bool basis_gpu_active = false;
    bool pricing_gpu_active = false;
    bool initialized = false;
    bool workspace_persistent = false;
    bool adaptive_gpu_eligible = false;
    bool basis_gpu_recommended = false;
    bool pricing_gpu_recommended = false;
    const char* adaptive_basis_backend = "CPU";
    const char* adaptive_pricing_backend = "CPU";
    std::size_t ftran_calls = 0;
    std::size_t btran_calls = 0;
    std::size_t pricing_calls = 0;
    std::size_t coordination_calls = 0;
    std::size_t update_count = 0;
    std::size_t workspace_allocations = 0;
    std::size_t workspace_reuses = 0;
    std::size_t adaptive_decisions = 0;
    std::size_t async_submitted = 0;
    std::size_t async_completed = 0;
    std::size_t async_in_flight = 0;
    bool async_available = true;
    bool async_gpu_capable = false;
    const char* async_backend = "CPU-ASYNC";
  };

  explicit UnifiedGpuSolverPipeline(Options options = {});

  bool initialize_basis(const SparseColumns&, Index dimension);
  bool initialize_pricing(const std::vector<std::size_t>& column_offsets,
                          const std::vector<std::size_t>& row_indices,
                          const std::vector<Real>& values,
                          const std::vector<Real>& objective);

  bool ftran(const std::vector<Real>& rhs, std::vector<Real>& solution);
  bool btran(const std::vector<Real>& rhs, std::vector<Real>& solution);
  bool price(const std::vector<Real>& dual, std::vector<Real>& reduced_costs);
  bool update(const std::vector<Real>& direction, Index leaving_row);

  // Coordinates the canonical simplex linear-algebra sequence:
  // B^T dual = btran_rhs, then reduced costs = c - A^T dual.
  bool btran_and_price(const std::vector<Real>& btran_rhs,
                       std::vector<Real>& dual,
                       std::vector<Real>& reduced_costs);

  // Coordinates reduced-cost pricing followed by FTRAN of an entering
  // column. This is the hot path after an entering variable is selected.
  bool price_and_ftran(const std::vector<Real>& dual,
                       const std::vector<Real>& entering_column,
                       std::vector<Real>& reduced_costs,
                       std::vector<Real>& direction);

  // Coordinates one complete linear-algebra iteration before the ratio test:
  // BTRAN -> pricing -> FTRAN.
  bool coordinate_iteration(const std::vector<Real>& btran_rhs,
                            const std::vector<Real>& entering_column,
                            std::vector<Real>& dual,
                            std::vector<Real>& reduced_costs,
                            std::vector<Real>& direction);

  bool initialized() const noexcept;
  bool gpu_active() const noexcept;
  bool basis_gpu_active() const noexcept;
  bool pricing_gpu_active() const noexcept;
  bool workspace_persistent() const noexcept;
  const char* basis_backend_name() const noexcept;
  const char* pricing_backend_name() const noexcept;
  const char* basis_adaptive_backend() const noexcept;
  const char* pricing_adaptive_backend() const noexcept;
  struct AsyncIterationResult {
    bool success = false;
    std::vector<Real> dual;
    std::vector<Real> reduced_costs;
    std::vector<Real> direction;
  };

  struct AsyncIteration {
    AsyncExecutionEngine::TaskId task = 0;
    std::shared_ptr<AsyncIterationResult> result;
  };

  // Dispatches one complete BTRAN -> pricing -> FTRAN iteration without
  // blocking the caller. The shared persistent workspace is serialized
  // internally so asynchronous submission cannot race the reusable buffers.
  AsyncIteration coordinate_iteration_async(
      const std::vector<Real>& btran_rhs,
      const std::vector<Real>& entering_column);

  bool wait_async(AsyncIteration& operation);
  bool async_ready(const AsyncIteration& operation) const;
  bool wait_all_async();

  Report report() const noexcept;

private:
  Options options_;
  AdaptiveBackendSelector adaptive_selector_;
  AdaptiveBackendSelector::Decision basis_decision_;
  AdaptiveBackendSelector::Decision pricing_decision_;
  basis::SimplexBasisBackend basis_;
  gpu::SparsePricingWorkspace pricing_;
  UnifiedGpuWorkspace workspace_;
  bool basis_initialized_ = false;
  bool pricing_initialized_ = false;
  std::size_t ftran_calls_ = 0;
  std::size_t btran_calls_ = 0;
  std::size_t pricing_calls_ = 0;
  std::size_t coordination_calls_ = 0;
  std::size_t adaptive_decisions_ = 0;
  AsyncExecutionEngine async_engine_;
  mutable std::mutex async_operation_mutex_;
};

}  // namespace indigenous::pipeline
