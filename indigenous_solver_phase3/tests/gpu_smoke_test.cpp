#include "gpu/vector_ops.hpp"

#include <cmath>
#include <iostream>
#include <vector>

int main() {
  const bool cuda_available = indigenous::gpu::available();
  std::cout << "Backend runtime: " << indigenous::gpu::runtime_status() << "\n";

  std::vector<float> x{1.0f, 2.0f, 3.0f, 4.0f};
  std::vector<float> y{10.0f, 20.0f, 30.0f, 40.0f};

  if(!indigenous::gpu::axpy(2.0f, x, y)) {
    std::cerr << "AXPY operation failed\n";
    return 1;
  }

  const std::vector<float> expected{12.0f, 24.0f, 36.0f, 48.0f};
  for(std::size_t i = 0; i < expected.size(); ++i) {
    if(std::abs(y[i] - expected[i]) > 1e-6f) {
      std::cerr << "AXPY mismatch at " << i << "\n";
      return 1;
    }
  }

  if(cuda_available) {
    std::cout << "CUDA AXPY smoke test: PASS\n";
  } else {
    std::cout << "CPU fallback AXPY smoke test: PASS\n";
  }
  return 0;
}
