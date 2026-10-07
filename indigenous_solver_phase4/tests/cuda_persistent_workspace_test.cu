#include "gpu/cuda_basis_workspace.hpp"

#include <cuda_runtime.h>

#include <cstdint>
#include <iostream>
#include <utility>
#include <vector>

int main() {
  int device_count = 0;
  if (cudaGetDeviceCount(&device_count) != cudaSuccess || device_count <= 0) {
    std::cout << "Phase 4.8 persistent workspace validation: SKIP (no CUDA device)\n";
    return 0;
  }

  indigenous::basis::BasisFactorization factorization;
  factorization.lower = {
      {},
      {{0, 2.0}},
      {{0, -1.0}, {1, 3.0}}
  };
  factorization.upper = {
      {{1, 1.0}},
      {{2, 4.0}},
      {}
  };
  factorization.diagonal = {2.0, 3.0, 5.0};
  factorization.permutation = {0, 2, 1};

  indigenous::gpu::CudaBasisWorkspace workspace;
  if (!workspace.initialize(factorization)) {
    std::cerr << "Persistent workspace initialization: FAIL\n";
    return 1;
  }

  if (!workspace.workspace_persistent() ||
      workspace.workspace_allocations() != 1 ||
      workspace.workspace_reuses() != 0) {
    std::cerr << "Initial persistent workspace state: FAIL\n";
    return 1;
  }

  const std::vector<double> rhs = {3.0, -2.0, 7.0};
  std::vector<double> solution;

  if (!workspace.ftran(rhs, solution)) {
    std::cerr << "First FTRAN: FAIL\n";
    return 1;
  }
  if (workspace.ftran_calls() != 1 ||
      workspace.workspace_allocations() != 1 ||
      workspace.workspace_reuses() != 0) {
    std::cerr << "FTRAN workspace reuse accounting: FAIL\n";
    return 1;
  }

  if (!workspace.ftran(rhs, solution)) {
    std::cerr << "Second FTRAN: FAIL\n";
    return 1;
  }
  if (workspace.ftran_calls() != 2 ||
      workspace.workspace_allocations() != 1 ||
      workspace.workspace_reuses() != 1) {
    std::cerr << "Repeated FTRAN persistence: FAIL\n";
    return 1;
  }

  if (!workspace.btran(rhs, solution)) {
    std::cerr << "First BTRAN: FAIL\n";
    return 1;
  }
  if (workspace.btran_calls() != 1 ||
      workspace.workspace_allocations() != 1 ||
      workspace.workspace_reuses() != 2) {
    std::cerr << "Cross-solve workspace persistence: FAIL\n";
    return 1;
  }

  std::cout << "Phase 4.8 persistent workspace validation: PASS\n";
  std::cout << "  allocations: " << workspace.workspace_allocations() << "\n";
  std::cout << "  reuses: " << workspace.workspace_reuses() << "\n";
  std::cout << "  FTRAN calls: " << workspace.ftran_calls() << "\n";
  std::cout << "  BTRAN calls: " << workspace.btran_calls() << "\n";
  return 0;
}
