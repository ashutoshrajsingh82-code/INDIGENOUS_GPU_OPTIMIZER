#include "gpu/cuda_basis_workspace.hpp"
#include <cuda_runtime.h>
#include <iostream>

int main() {
  using indigenous::basis::BasisFactorization;
  using indigenous::gpu::CudaBasisWorkspace;

  int device_count = 0;
  if (cudaGetDeviceCount(&device_count) != cudaSuccess || device_count <= 0) {
    std::cout << "Phase 4.5 CUDA basis representation: SKIP (no CUDA device)\n";
    return 0;
  }

  BasisFactorization factorization;
  factorization.lower = {{}, {{0, 2.0}}, {{0, -1.0}, {1, 3.0}}};
  factorization.upper = {{{1, 1.0}}, {{2, 4.0}}, {}};
  factorization.diagonal = {2.0, 3.0, 5.0};
  factorization.permutation = {0, 2, 1};

  CudaBasisWorkspace workspace;
  if (!workspace.initialize(factorization)) {
    std::cerr << "CUDA basis representation initialization: FAIL\n";
    return 1;
  }

  if (!workspace.valid() || !workspace.device_ready() || workspace.size() != 3 ||
      workspace.lower_nnz() != 3 || workspace.upper_nnz() != 2) {
    std::cerr << "CUDA basis representation metadata: FAIL\n";
    return 1;
  }

  std::cout << "Phase 4.5 CUDA basis representation: PASS\n";
  std::cout << "  dimension: " << workspace.size() << "\n";
  std::cout << "  L nnz: " << workspace.lower_nnz() << "\n";
  std::cout << "  U nnz: " << workspace.upper_nnz() << "\n";
  std::cout << "  upload ms: " << workspace.last_upload_ms() << "\n";
  return 0;
}
