#include "gpu/sparse_pricing.hpp"

#include <chrono>
#include <utility>

namespace indigenous::gpu {

struct SparsePricingWorkspace::Impl {
  std::vector<std::size_t> offsets;
  std::vector<std::size_t> rows;
  std::vector<double> values;
  std::vector<double> objective;
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
  reduced_costs.assign(impl_->objective.size(),0.0);
  for(std::size_t j=0;j<impl_->objective.size();++j){
    double rc=impl_->objective[j];
    for(std::size_t p=impl_->offsets[j];p<impl_->offsets[j+1];++p){
      if(impl_->rows[p]>=dual.size()) return false;
      rc-=impl_->values[p]*dual[impl_->rows[p]];
    }
    reduced_costs[j]=rc;
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
