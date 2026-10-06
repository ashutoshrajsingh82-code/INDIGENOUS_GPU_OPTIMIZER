#pragma once
#include <string>
#include <vector>

namespace indigenous::gpu {

// True when a CUDA-capable GPU backend is available at runtime.
bool available();

// Human-readable runtime/backend diagnostic. Useful for startup logs and
// benchmark reports; does not change backend selection.
std::string runtime_status();

// y = alpha*x + y.
// Uses CUDA when compiled with the CUDA backend; otherwise uses the CPU fallback.
bool axpy(float alpha, const std::vector<float>& x, std::vector<float>& y);

} // namespace indigenous::gpu
