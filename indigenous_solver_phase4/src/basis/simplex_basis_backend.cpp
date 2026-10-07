#include "basis/simplex_basis_backend.hpp"

#include <utility>

namespace indigenous::basis {

SimplexBasisBackend::SimplexBasisBackend(Options options)
    : options_(options),
      active_(make_cpu_basis_solver()),
      cpu_(make_cpu_basis_solver()) {
  gpu_active_ = false;
}

bool SimplexBasisBackend::initialize(const BasisSolver::SparseColumns& basis_columns,
                                     BasisSolver::Index dimension) {
  basis_columns_ = basis_columns;
  dimension_ = dimension;
  if (!active_) active_ = make_cpu_basis_solver();
  if (!cpu_) cpu_ = make_cpu_basis_solver();
  if (!active_->initialize(basis_columns, dimension, options_.pivot_tolerance))
    return false;
  if (cpu_ && cpu_.get() != active_.get())
    cpu_->initialize(basis_columns, dimension, options_.pivot_tolerance);
  return true;
}

bool SimplexBasisBackend::ftran(const std::vector<BasisSolver::Real>& rhs,
                                std::vector<BasisSolver::Real>& solution) {
  return active_ && active_->ftran(rhs, solution);
}

bool SimplexBasisBackend::btran(const std::vector<BasisSolver::Real>& rhs,
                                std::vector<BasisSolver::Real>& solution) {
  return active_ && active_->btran(rhs, solution);
}

bool SimplexBasisBackend::update(
    const std::vector<BasisSolver::Real>& direction,
    BasisSolver::Index leaving_row) {
  if (!active_) return false;
  if (active_->update(direction, leaving_row, options_.pivot_tolerance))
    return true;

  // A CUDA basis workspace is intentionally immutable between uploads in
  // Phase 4.8. If a future GPU implementation becomes active here, a live
  // simplex pivot must switch to the CPU reference until GPU-side basis
  // updates are available. This prevents stale GPU factors from affecting
  // simplex correctness.
  if (gpu_active_) {
    if (!activate_cpu()) return false;
    return active_->update(direction, leaving_row,
                           options_.pivot_tolerance);
  }
  return false;
}

bool SimplexBasisBackend::activate_cpu() {
  if (active_ == cpu_) return true;
  if (!cpu_) return false;
  if (!cpu_->initialize(basis_columns_, dimension_, options_.pivot_tolerance))
    return false;
  active_ = std::move(cpu_);
  gpu_active_ = false;
  return active_ != nullptr;
}

bool SimplexBasisBackend::valid() const noexcept {
  return active_ && active_->valid();
}

const char* SimplexBasisBackend::backend_name() const noexcept {
  return active_ ? active_->backend_name() : "NONE";
}

bool SimplexBasisBackend::gpu_active() const noexcept {
  return gpu_active_;
}

std::size_t SimplexBasisBackend::update_count() const noexcept {
  return active_ ? active_->update_count() : 0;
}

}  // namespace indigenous::basis
