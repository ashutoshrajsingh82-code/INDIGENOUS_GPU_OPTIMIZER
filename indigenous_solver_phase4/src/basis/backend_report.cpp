#include "basis/backend_report.hpp"

#include "basis/simplex_basis_backend.hpp"

#include <sstream>

#ifndef INDIGENOUS_PHASE4_CUDA_COMPILED
#define INDIGENOUS_PHASE4_CUDA_COMPILED 0
#endif

namespace indigenous::basis {

BackendReport make_backend_report(const SimplexBasisBackend& backend) {
  BackendReport report;
  report.backend = backend.backend_name();
  report.gpu_active = backend.gpu_active();
  report.execution = report.gpu_active ? "GPU" : "CPU";
  report.update_count = backend.update_count();

  // Capability and active execution are deliberately separate. A CPU-only
  // build must never imply that CUDA work occurred.
  report.cuda_compiled = INDIGENOUS_PHASE4_CUDA_COMPILED != 0;
  report.device_ready = false;
  return report;
}

std::string format_backend_report(const BackendReport& report) {
  std::ostringstream out;
  out << "Phase 4.12 backend report: "
      << "Backend=" << report.backend
      << " | Execution=" << report.execution
      << " | CUDA compiled=" << (report.cuda_compiled ? "YES" : "NO")
      << " | Device ready=" << (report.device_ready ? "YES" : "NO")
      << " | FTRAN calls=" << report.ftran_calls
      << " | BTRAN calls=" << report.btran_calls
      << " | Updates=" << report.update_count
      << " | Workspace allocations=" << report.workspace_allocations
      << " | Workspace reuses=" << report.workspace_reuses;
  return out.str();
}

}  // namespace indigenous::basis
