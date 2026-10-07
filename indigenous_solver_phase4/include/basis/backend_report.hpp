#pragma once

#include <cstddef>
#include <string>

namespace indigenous::basis {

class SimplexBasisBackend;

struct BackendReport {
  std::string backend = "UNKNOWN";
  std::string execution = "UNKNOWN";
  bool gpu_active = false;
  bool cuda_compiled = false;
  bool device_ready = false;
  std::size_t ftran_calls = 0;
  std::size_t btran_calls = 0;
  std::size_t update_count = 0;
  std::size_t workspace_allocations = 0;
  std::size_t workspace_reuses = 0;
};

BackendReport make_backend_report(const SimplexBasisBackend& backend);

// Stable one-line status intended for CLI/benchmark logs.
std::string format_backend_report(const BackendReport& report);

}  // namespace indigenous::basis
