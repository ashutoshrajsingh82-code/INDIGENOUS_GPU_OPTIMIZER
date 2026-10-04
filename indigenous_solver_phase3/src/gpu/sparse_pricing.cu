#include "gpu/sparse_pricing.hpp"

#include <cuda_runtime.h>
#include <utility>

namespace indigenous::gpu {
namespace {

__global__ void sparse_reduced_costs_kernel(
    const std::size_t* column_offsets,
    const std::size_t* row_indices,
    const double* values,
    const double* objective,
    const double* dual,
    double* reduced_costs,
    std::size_t column_count) {
  const std::size_t j =
      static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
  if(j >= column_count) return;
  double rc=objective[j];
  for(std::size_t p=column_offsets[j];p<column_offsets[j+1];++p)
    rc-=values[p]*dual[row_indices[p]];
  reduced_costs[j]=rc;
}

} // namespace

struct SparsePricingWorkspace::Impl {
  std::size_t column_count=0;
  std::size_t dual_capacity=0;
  std::size_t* d_offsets=nullptr;
  std::size_t* d_rows=nullptr;
  double* d_values=nullptr;
  double* d_objective=nullptr;
  double* d_dual=nullptr;
  double* d_result=nullptr;
  bool initialized=false;
};

static void free_device(SparsePricingWorkspace::Impl* p) {
  if(!p) return;
  cudaFree(p->d_offsets);
  cudaFree(p->d_rows);
  cudaFree(p->d_values);
  cudaFree(p->d_objective);
  cudaFree(p->d_dual);
  cudaFree(p->d_result);
  p->d_offsets=nullptr; p->d_rows=nullptr; p->d_values=nullptr;
  p->d_objective=nullptr; p->d_dual=nullptr; p->d_result=nullptr;
  p->dual_capacity=0; p->initialized=false;
}

SparsePricingWorkspace::~SparsePricingWorkspace() {
  if(impl_){ free_device(impl_); delete impl_; }
}

SparsePricingWorkspace::SparsePricingWorkspace(SparsePricingWorkspace&& other) noexcept
    : impl_(other.impl_) { other.impl_=nullptr; }

SparsePricingWorkspace& SparsePricingWorkspace::operator=(SparsePricingWorkspace&& other) noexcept {
  if(this!=&other){
    if(impl_){ free_device(impl_); delete impl_; }
    impl_=other.impl_; other.impl_=nullptr;
  }
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
  free_device(impl_);

  impl_->column_count=objective.size();
  const std::size_t offset_bytes=column_offsets.size()*sizeof(std::size_t);
  const std::size_t nnz_bytes=values.size()*sizeof(double);
  const std::size_t row_bytes=row_indices.size()*sizeof(std::size_t);
  const std::size_t column_bytes=objective.size()*sizeof(double);

  if(cudaMalloc(reinterpret_cast<void**>(&impl_->d_offsets),offset_bytes)!=cudaSuccess) return false;
  if(!row_indices.empty() &&
     cudaMalloc(reinterpret_cast<void**>(&impl_->d_rows),row_bytes)!=cudaSuccess){
    free_device(impl_); return false;
  }
  if(!values.empty() &&
     cudaMalloc(reinterpret_cast<void**>(&impl_->d_values),nnz_bytes)!=cudaSuccess){
    free_device(impl_); return false;
  }
  if(cudaMalloc(reinterpret_cast<void**>(&impl_->d_objective),column_bytes)!=cudaSuccess){
    free_device(impl_); return false;
  }
  if(cudaMalloc(reinterpret_cast<void**>(&impl_->d_result),column_bytes)!=cudaSuccess){
    free_device(impl_); return false;
  }
  if(cudaMemcpy(impl_->d_offsets,column_offsets.data(),offset_bytes,cudaMemcpyHostToDevice)!=cudaSuccess ||
     (!row_indices.empty() && cudaMemcpy(impl_->d_rows,row_indices.data(),row_bytes,cudaMemcpyHostToDevice)!=cudaSuccess) ||
     (!values.empty() && cudaMemcpy(impl_->d_values,values.data(),nnz_bytes,cudaMemcpyHostToDevice)!=cudaSuccess) ||
     cudaMemcpy(impl_->d_objective,objective.data(),column_bytes,cudaMemcpyHostToDevice)!=cudaSuccess){
    free_device(impl_); return false;
  }
  impl_->initialized=true;
  return true;
}

bool SparsePricingWorkspace::compute(
    const std::vector<double>& dual,
    std::vector<double>& reduced_costs) {
  if(!impl_ || !impl_->initialized) return false;
  if(dual.empty()) return false;
  if(dual.size()>impl_->dual_capacity){
    if(impl_->d_dual) cudaFree(impl_->d_dual);
    if(cudaMalloc(reinterpret_cast<void**>(&impl_->d_dual),dual.size()*sizeof(double))!=cudaSuccess){
      impl_->d_dual=nullptr; return false;
    }
    impl_->dual_capacity=dual.size();
  }
  if(cudaMemcpy(impl_->d_dual,dual.data(),dual.size()*sizeof(double),cudaMemcpyHostToDevice)!=cudaSuccess)
    return false;
  constexpr unsigned threads=256;
  const unsigned blocks=static_cast<unsigned>((impl_->column_count+threads-1)/threads);
  sparse_reduced_costs_kernel<<<blocks,threads>>>(
      impl_->d_offsets,impl_->d_rows,impl_->d_values,impl_->d_objective,
      impl_->d_dual,impl_->d_result,impl_->column_count);
  if(cudaGetLastError()!=cudaSuccess || cudaDeviceSynchronize()!=cudaSuccess) return false;
  reduced_costs.resize(impl_->column_count);
  return cudaMemcpy(reduced_costs.data(),impl_->d_result,
                    impl_->column_count*sizeof(double),cudaMemcpyDeviceToHost)==cudaSuccess;
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
