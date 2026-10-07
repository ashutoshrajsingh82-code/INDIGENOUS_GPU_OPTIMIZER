#include "basis/backend_report.hpp"
#include "basis/simplex_basis_backend.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

int main() {
  using Backend = indigenous::basis::SimplexBasisBackend;
  using Real = indigenous::basis::BasisSolver::Real;
  using Index = indigenous::basis::BasisSolver::Index;

  Backend::Options options;
  options.prefer_gpu = false;
  options.pivot_tolerance = 1e-12;
  Backend backend(options);

  const std::vector<std::vector<std::pair<Index, Real>>> basis = {
      {{0, 2.0}},
      {{0, 1.0}, {1, 1.0}}
  };
  assert(backend.initialize(basis, 2));

  std::vector<Real> rhs = {5.0, 2.0};
  std::vector<Real> x;
  assert(backend.ftran(rhs, x));

  const auto report = indigenous::basis::make_backend_report(backend);
  assert(report.backend == "CPU");
  assert(report.execution == "CPU");
  assert(!report.gpu_active);
  assert(!report.cuda_compiled);
  assert(!report.device_ready);
  assert(report.update_count == 0);

  const std::string line = indigenous::basis::format_backend_report(report);
  assert(line.find("Phase 4.12 backend report:") != std::string::npos);
  assert(line.find("Backend=CPU") != std::string::npos);
  assert(line.find("Execution=CPU") != std::string::npos);
  assert(line.find("CUDA compiled=NO") != std::string::npos);
  assert(line.find("Device ready=NO") != std::string::npos);

  std::cout << line << "\n";
  std::cout << "Phase 4.12 backend reporting test: PASS\n";
  return 0;
}
