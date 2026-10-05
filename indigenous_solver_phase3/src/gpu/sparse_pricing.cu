#include <chrono>
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
  cudaStream_t stream=nullptr;
  cudaEvent_t h2d_start=nullptr;
  cudaEvent_t h2d_end=nullptr;
  cudaEvent_t kernel_start=nullptr;
  cudaEvent_t kernel_end=nullptr;
  cudaEvent_t d2h_start=nullptr;
  cudaEvent_t d2h_end=nullptr;
  double initialize_ms=0;
  double compute_ms=0;
  double host_to_device_ms=0;
  double kernel_ms=0;
  double device_to_host_ms=0;
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
  if(p->h2d_start) cudaEventDestroy(p->h2d_start);
  if(p->h2d_end) cudaEventDestroy(p->h2d_end);
  if(p->kernel_start) cudaEventDestroy(p->kernel_start);
  if(p->kernel_end) cudaEventDestroy(p->kernel_end);
  if(p->d2h_start) cudaEventDestroy(p->d2h_start);
  if(p->d2h_end) cudaEventDestroy(p->d2h_end);
  if(p->stream) cudaStreamDestroy(p->stream);
  p->d_offsets=nullptr; p->d_rows=nullptr; p->d_values=nullptr;
  p->d_objective=nullptr; p->d_dual=nullptr; p->d_result=nullptr;
  p->dual_capacity=0; p->stream=nullptr;
  p->h2d_start=nullptr; p->h2d_end=nullptr;
  p->kernel_start=nullptr; p->kernel_end=nullptr;
  p->d2h_start=nullptr; p->d2h_end=nullptr;
  p->initialize_ms=0; p->compute_ms=0;
  p->host_to_device_ms=0; p->kernel_ms=0; p->device_to_host_ms=0;
  p->initialized=false;
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
  const auto initialize_start=std::chrono::steady_clock::now();
  if(cudaStreamCreateWithFlags(&impl_->stream,cudaStreamNonBlocking)!=cudaSuccess){
    free_device(impl_); return false;
  }
  if(cudaEventCreate(&impl_->h2d_start)!=cudaSuccess ||
     cudaEventCreate(&impl_->h2d_end)!=cudaSuccess ||
     cudaEventCreate(&impl_->kernel_start)!=cudaSuccess ||
     cudaEventCreate(&impl_->kernel_end)!=cudaSuccess ||
     cudaEventCreate(&impl_->d2h_start)!=cudaSuccess ||
     cudaEventCreate(&impl_->d2h_end)!=cudaSuccess){
    free_device(impl_); return false;
  }
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
  if(cudaEventRecord(impl_->h2d_start,impl_->stream)!=cudaSuccess ||
     cudaMemcpyAsync(impl_->d_offsets,column_offsets.data(),offset_bytes,cudaMemcpyHostToDevice,impl_->stream)!=cudaSuccess ||
     (!row_indices.empty() && cudaMemcpyAsync(impl_->d_rows,row_indices.data(),row_bytes,cudaMemcpyHostToDevice,impl_->stream)!=cudaSuccess) ||
     (!values.empty() && cudaMemcpyAsync(impl_->d_values,values.data(),nnz_bytes,cudaMemcpyHostToDevice,impl_->stream)!=cudaSuccess) ||
     cudaMemcpyAsync(impl_->d_objective,objective.data(),column_bytes,cudaMemcpyHostToDevice,impl_->stream)!=cudaSuccess){
    free_device(impl_); return false;
  }
  if(cudaEventRecord(impl_->h2d_end,impl_->stream)!=cudaSuccess ||
     cudaStreamSynchronize(impl_->stream)!=cudaSuccess) {
    free_device(impl_); return false;
  }
  impl_->initialize_ms=std::chrono::duration<double,std::milli>(
      std::chrono::steady_clock::now()-initialize_start).count();
  impl_->initialized=true;
  return true;
}

bool SparsePricingWorkspace::compute(
    const std::vector<double>& dual,
    std::vector<double>& reduced_costs) {
  if(!impl_ || !impl_->initialized) return false;
  if(dual.empty()) return false;
  const auto compute_start=std::chrono::steady_clock::now();
  if(dual.size()>impl_->dual_capacity){
    if(impl_->d_dual) cudaFree(impl_->d_dual);
    if(cudaMalloc(reinterpret_cast<void**>(&impl_->d_dual),dual.size()*sizeof(double))!=cudaSuccess){
      impl_->d_dual=nullptr; return false;
    }
    impl_->dual_capacity=dual.size();
  }
  if(cudaEventRecord(impl_->h2d_start,impl_->stream)!=cudaSuccess ||
     cudaMemcpyAsync(impl_->d_dual,dual.data(),dual.size()*sizeof(double),cudaMemcpyHostToDevice,impl_->stream)!=cudaSuccess ||
     cudaEventRecord(impl_->h2d_end,impl_->stream)!=cudaSuccess)
    return false;
  constexpr unsigned threads=256;
  const unsigned blocks=static_cast<unsigned>((impl_->column_count+threads-1)/threads);
  if(cudaEventRecord(impl_->kernel_start,impl_->stream)!=cudaSuccess) return false;
  sparse_reduced_costs_kernel<<<blocks,threads,0,impl_->stream>>>(
      impl_->d_offsets,impl_->d_rows,impl_->d_values,impl_->d_objective,
      impl_->d_dual,impl_->d_result,impl_->column_count);
  if(cudaGetLastError()!=cudaSuccess) return false;
  if(cudaEventRecord(impl_->kernel_end,impl_->stream)!=cudaSuccess) return false;
  reduced_costs.resize(impl_->column_count);
  if(cudaEventRecord(impl_->d2h_start,impl_->stream)!=cudaSuccess ||
     cudaMemcpyAsync(reduced_costs.data(),impl_->d_result,
                     impl_->column_count*sizeof(double),cudaMemcpyDeviceToHost,
                     impl_->stream)!=cudaSuccess ||
     cudaEventRecord(impl_->d2h_end,impl_->stream)!=cudaSuccess) return false;
  // Simplex needs the reduced costs immediately, so this synchronization is
  // the unavoidable dependency boundary. The stream still keeps all GPU work
  // ordered and prepares the backend for future overlap with independent work.
  if(cudaStreamSynchronize(impl_->stream)!=cudaSuccess) return false;
  if(cudaEventElapsedTime(reinterpret_cast<float*>(&impl_->host_to_device_ms),impl_->h2d_start,impl_->h2d_end)!=cudaSuccess) return false;
  if(cudaEventElapsedTime(reinterpret_cast<float*>(&impl_->kernel_ms),impl_->kernel_start,impl_->kernel_end)!=cudaSuccess) return false;
  if(cudaEventElapsedTime(reinterpret_cast<float*>(&impl_->device_to_host_ms),impl_->d2h_start,impl_->d2h_end)!=cudaSuccess) return false;
  impl_->compute_ms=std::chrono::duration<double,std::milli>(
      std::chrono::steady_clock::now()-compute_start).count();
  return true;
}

double SparsePricingWorkspace::last_initialize_ms() const noexcept { return impl_ ? impl_->initialize_ms : 0.0; }
double SparsePricingWorkspace::last_compute_ms() const noexcept { return impl_ ? impl_->compute_ms : 0.0; }

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
