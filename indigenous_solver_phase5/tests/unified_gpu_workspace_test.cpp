#include <cmath>
#include <iostream>
#include <string>

#include "pipeline/unified_gpu_workspace.hpp"

namespace {
bool check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << "\n";
    return false;
  }
  return true;
}
}

int main() {
  using indigenous::pipeline::UnifiedGpuWorkspace;

  UnifiedGpuWorkspace workspace;
  if (!check(!workspace.initialized(), "workspace starts uninitialized")) return 1;
  if (!check(!workspace.ensure(UnifiedGpuWorkspace::BufferKind::Ftran, 2),
             "ensure rejects uninitialized workspace")) return 1;

  if (!check(workspace.initialize(2), "initialize")) return 1;
  if (!check(workspace.initialized(), "initialized flag")) return 1;
  if (!check(workspace.dimension() == 2, "dimension")) return 1;

  if (!check(workspace.ensure(UnifiedGpuWorkspace::BufferKind::Ftran, 2), "FTRAN allocation")) return 1;
  if (!check(workspace.ensure(UnifiedGpuWorkspace::BufferKind::Btran, 2), "BTRAN allocation")) return 1;
  if (!check(workspace.ensure(UnifiedGpuWorkspace::BufferKind::Pricing, 3), "pricing allocation")) return 1;
  if (!check(workspace.ensure(UnifiedGpuWorkspace::BufferKind::Pivot, 2), "pivot allocation")) return 1;
  if (!check(workspace.allocations() == 4, "four initial allocations")) return 1;
  if (!check(workspace.reuses() == 0, "no initial reuses")) return 1;

  workspace.ftran_buffer()[0] = 7.0;
  workspace.ftran_buffer()[1] = 9.0;

  if (!check(workspace.ensure(UnifiedGpuWorkspace::BufferKind::Ftran, 2), "FTRAN reuse")) return 1;
  if (!check(workspace.ensure(UnifiedGpuWorkspace::BufferKind::Btran, 2), "BTRAN reuse")) return 1;
  if (!check(workspace.ensure(UnifiedGpuWorkspace::BufferKind::Pricing, 3), "pricing reuse")) return 1;
  if (!check(workspace.ensure(UnifiedGpuWorkspace::BufferKind::Pivot, 2), "pivot reuse")) return 1;
  if (!check(workspace.allocations() == 4, "reuse does not allocate")) return 1;
  if (!check(workspace.reuses() == 4, "four reuses")) return 1;
  if (!check(workspace.ftran_buffer()[0] == 7.0 &&
             workspace.ftran_buffer()[1] == 9.0, "buffer contents persist")) return 1;

  if (!check(workspace.ensure(UnifiedGpuWorkspace::BufferKind::Pricing, 8),
             "pricing growth")) return 1;
  if (!check(workspace.allocations() == 5, "pricing growth counts one allocation")) return 1;
  if (!check(workspace.ensure(UnifiedGpuWorkspace::BufferKind::Pricing, 4),
             "smaller pricing request")) return 1;
  if (!check(workspace.reuses() == 5, "smaller request reuses capacity")) return 1;

  const auto report = workspace.report();
  if (!check(report.persistent, "workspace is persistent")) return 1;
  if (!check(report.allocations == 5, "report allocation count")) return 1;
  if (!check(report.reuses == 5, "report reuse count")) return 1;
  if (!check(report.ftran_capacity >= 2, "FTRAN capacity")) return 1;
  if (!check(report.pricing_capacity >= 8, "pricing capacity")) return 1;

  workspace.release();
  if (!check(!workspace.initialized(), "release clears initialized state")) return 1;
  if (!check(workspace.allocations() == 0 && workspace.reuses() == 0,
             "release clears counters")) return 1;

  std::cout << "Phase 5.2 unified persistent workspace: PASS\n";
  std::cout << "Allocations: " << report.allocations
            << " | Reuses: " << report.reuses
            << " | Persistent: YES\n";
  return 0;
}
