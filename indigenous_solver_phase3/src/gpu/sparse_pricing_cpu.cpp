#include "gpu/sparse_pricing.hpp"

#include <algorithm>
#include <chrono>
#include <utility>

namespace indigenous::gpu {

struct SparsePricingWorkspace::Impl {
  std::vector<std::size_t> offsets;
  std::vector<std::size_t> rows;
  std::vector<double> values;
  std::vector<double> objective;
  std::size_t max_row_index=0;
  bool initialized=false;
  double initialize_ms=0;
  double compute_ms=0;
};

SparsePricingWorkspace::~SparsePricingWorkspace() { delete impl_; }

SparsePricingWorkspace::SparsePricingWorkspace(SparsePricingWorkspace&& other) noexcept
    : impl_(other.impl_) { other.impl_=nullptr; }

SparsePricingWorkspace& SparsePricingWorkspace::operator=(SparsePricingWorkspace&& other) noexcept {
  if(this!=&other){ delete impl_; impl_=other.impl_; other.impl_=nullptr; }
  return *this;
}

bool SparsePricingWorkspace::initialize(
    const std::vector<std::size_t>& column_offsets,
    const std::vector<std::size_t>& row_indices,
    const std::vector<double>& values,
    const std::vector<double>& objective) {
  if(column_offsets.empty() || column_offsets.back()!=values.size() ||
     row_indices.size()!=values.size() ||
     objective.size()+1!=column_offsets.size()) return false;
  if(!impl_) impl_=new Impl();
  const auto start=std::chrono::steady_clock::now();
  impl_->offsets=column_offsets;
  impl_->rows=row_indices;
  impl_->values=values;
  impl_->objective=objective;
  impl_->max_row_index=0;
  for(const std::size_t row : impl_->rows)
    impl_->max_row_index=std::max(impl_->max_row_index,row);
  impl_->initialized=true;
  impl_->initialize_ms=std::chrono::duration<double,std::milli>(
      std::chrono::steady_clock::now()-start).count();
  return true;
}

bool SparsePricingWorkspace::compute(
    const std::vector<double>& dual,
    std::vector<double>& reduced_costs) {
  if(!impl_ || !impl_->initialized) return false;
  const auto start=std::chrono::steady_clock::now();
  if(!impl_->rows.empty() && impl_->max_row_index>=dual.size()) return false;

  const std::size_t column_count=impl_->objective.size();
  reduced_costs.resize(column_count);

  const auto* offsets=impl_->offsets.data();
  const auto* rows=impl_->rows.data();
  const auto* values=impl_->values.data();
  const auto* objective=impl_->objective.data();
  const auto* dual_data=dual.data();
  auto* output=reduced_costs.data();

  // The workspace owns contiguous CSC arrays, so keep the hot loop to
  // pointer/index arithmetic and one multiply-subtract per nonzero. Row
  // bounds are validated once above instead of inside every nonzero visit.
  for(std::size_t j=0;j<column_count;++j){
    double rc=objective[j];
    for(std::size_t p=offsets[j];p<offsets[j+1];++p)
      rc-=values[p]*dual_data[rows[p]];
    output[j]=rc;
  }
  impl_->compute_ms=std::chrono::duration<double,std::milli>(
      std::chrono::steady_clock::now()-start).count();
  return true;
}

double SparsePricingWorkspace::last_initialize_ms() const noexcept {
  return impl_ ? impl_->initialize_ms : 0.0;
}

double SparsePricingWorkspace::last_compute_ms() const noexcept {
  return impl_ ? impl_->compute_ms : 0.0;
}
double SparsePricingWorkspace::last_host_to_device_ms() const noexcept { return 0.0; }
double SparsePricingWorkspace::last_kernel_ms() const noexcept { return 0.0; }
double SparsePricingWorkspace::last_device_to_host_ms() const noexcept { return 0.0; }

bool SparsePricingWorkspace::valid() const noexcept {
  return impl_!=nullptr && impl_->initialized;
}

bool sparse_reduced_costs(
    const std::vector<std::size_t>& column_offsets,
    const std::vector<std::size_t>& row_indices,
    const std::vector<double>& values,
    const std::vector<double>& objective,
    const std::vector<double>& dual,
    std::vector<double>& reduced_costs) {
  SparsePricingWorkspace workspace;
  return workspace.initialize(column_offsets,row_indices,values,objective) &&
         workspace.compute(dual,reduced_costs);
}

} // namespace indigenous::gpu
