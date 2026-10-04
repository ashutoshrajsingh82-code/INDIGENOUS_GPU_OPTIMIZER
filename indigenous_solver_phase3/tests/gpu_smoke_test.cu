#include "gpu/vector_ops.hpp"

#include <cmath>
#include <iostream>
#include <vector>

int main() {
  if(!indigenous::gpu::available()) {
    std::cout << "GPU backend unavailable\n";
    return 0;
  }

  std::vector<float> x{1.0f, 2.0f, 3.0f, 4.0f};
  std::vector<float> y{10.0f, 20.0f, 30.0f, 40.0f};

  if(!indigenous::gpu::axpy(2.0f, x, y)) {
    std::cerr << "GPU AXPY failed\n";
    return 1;
  }

  const std::vector<float> expected{12.0f, 24.0f, 36.0f, 48.0f};
  for(std::size_t i=0;i<expected.size();++i) {
    if(std::abs(y[i] - expected[i]) > 1e-6f) {
      std::cerr << "GPU AXPY mismatch at " << i << "\n";
      return 1;
    }
  }

  std::cout << "GPU AXPY smoke test: PASS\n";
  return 0;
}
