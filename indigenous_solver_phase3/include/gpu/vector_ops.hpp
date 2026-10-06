#pragma once
#include <vector>

namespace indigenous::gpu {

// True when a CUDA-capable GPU backend is available at runtime.
bool available();

// Returns the backend that is actually available at runtime: "CUDA" or "CPU".
const char* backend_name();

// y = alpha*x + y.
// Uses CUDA when compiled with the CUDA backend; otherwise uses the CPU fallback.
bool axpy(float alpha, const std::vector<float>& x, std::vector<float>& y);

} // namespace indigenous::gpu
