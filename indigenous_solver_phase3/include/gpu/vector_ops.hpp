#pragma once
#include <cstddef>
#include <vector>

namespace indigenous::gpu {

// Returns true when the CUDA backend was compiled with device support.
bool available();

// y = alpha*x + y on the GPU. Returns false on CUDA/runtime failure.
bool axpy(float alpha, const std::vector<float>& x, std::vector<float>& y);

} // namespace indigenous::gpu
