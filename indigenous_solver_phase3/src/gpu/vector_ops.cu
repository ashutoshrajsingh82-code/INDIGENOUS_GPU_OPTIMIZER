#include "gpu/vector_ops.hpp"

#include <cuda_runtime.h>

#include <cstddef>
#include <string>

namespace indigenous::gpu {
namespace {

__global__ void axpy_kernel(float alpha, const float* x, float* y, std::size_t n) {
  const std::size_t i = static_cast<std::size_t>(blockIdx.x) * blockDim.x + threadIdx.x;
  if(i < n) y[i] = alpha * x[i] + y[i];
}

} // namespace

bool available() {
  int count = 0;
  return cudaGetDeviceCount(&count) == cudaSuccess && count > 0;
}

std::string runtime_status() {
  int count = 0;
  const cudaError_t status = cudaGetDeviceCount(&count);
  if(status != cudaSuccess) {
    return std::string("CUDA runtime error: ") + cudaGetErrorString(status);
  }
  if(count <= 0) return "CUDA runtime: no CUDA-capable device detected";

  cudaDeviceProp prop{};
  if(cudaGetDeviceProperties(&prop, 0) != cudaSuccess)
    return "CUDA runtime: device detected, but device properties could not be queried";

  return std::string("CUDA runtime: ") + prop.name + " (" +
         std::to_string(prop.major) + "." + std::to_string(prop.minor) + ")";
}

bool axpy(float alpha, const std::vector<float>& x, std::vector<float>& y) {
  if(x.size() != y.size()) return false;
  if(x.empty()) return true;

  float* d_x = nullptr;
  float* d_y = nullptr;
  const std::size_t bytes = x.size() * sizeof(float);

  if(cudaMalloc(&d_x, bytes) != cudaSuccess) return false;
  if(cudaMalloc(&d_y, bytes) != cudaSuccess) {
    cudaFree(d_x);
    return false;
  }

  bool ok = true;
  ok = ok && cudaMemcpy(d_x, x.data(), bytes, cudaMemcpyHostToDevice) == cudaSuccess;
  ok = ok && cudaMemcpy(d_y, y.data(), bytes, cudaMemcpyHostToDevice) == cudaSuccess;

  if(ok) {
    const unsigned threads = 256;
    const unsigned blocks = static_cast<unsigned>((x.size() + threads - 1) / threads);
    axpy_kernel<<<blocks, threads>>>(alpha, d_x, d_y, x.size());
    ok = cudaGetLastError() == cudaSuccess;
    ok = ok && cudaDeviceSynchronize() == cudaSuccess;
  }

  if(ok) ok = cudaMemcpy(y.data(), d_y, bytes, cudaMemcpyDeviceToHost) == cudaSuccess;

  cudaFree(d_x);
  cudaFree(d_y);
  return ok;
}

} // namespace indigenous::gpu
