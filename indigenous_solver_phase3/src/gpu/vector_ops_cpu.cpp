#include "gpu/vector_ops.hpp"
#include <string>

namespace indigenous::gpu {

bool available() {
  return false;
}

std::string runtime_status() {
  return "CPU fallback backend (CUDA not compiled)";
}

bool axpy(float alpha, const std::vector<float>& x, std::vector<float>& y) {
  if(x.size() != y.size()) return false;
  for(std::size_t i = 0; i < x.size(); ++i) {
    y[i] = alpha * x[i] + y[i];
  }
  return true;
}

} // namespace indigenous::gpu
