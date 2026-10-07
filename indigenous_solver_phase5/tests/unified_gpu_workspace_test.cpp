#include <cassert>
#include <iostream>

#include "pipeline/unified_gpu_workspace.hpp"

int main() {
  using indigenous::pipeline::UnifiedGpuWorkspace;

  UnifiedGpuWorkspace workspace;
  assert(!workspace.initialized());
  assert(!workspace.ensure(UnifiedGpuWorkspace::BufferKind::Ftran, 2));

  assert(workspace.initialize(2));
  assert(workspace.initialized());
  assert(workspace.dimension() == 2);

  assert(workspace.ensure(UnifiedGpuWorkspace::BufferKind::Ftran, 2));
  assert(workspace.ensure(UnifiedGpuWorkspace::BufferKind::Btran, 2));
  assert(workspace.ensure(UnifiedGpuWorkspace::BufferKind::Pricing, 3));
  assert(workspace.ensure(UnifiedGpuWorkspace::BufferKind::Pivot, 2));
  assert(workspace.allocations() == 4);
  assert(workspace.reuses() == 0);

  workspace.ftran_buffer()[0] = 7.0;
  workspace.ftran_buffer()[1] = 9.0;

  // Same-size requests must reuse existing capacity rather than allocate.
  assert(workspace.ensure(UnifiedGpuWorkspace::BufferKind::Ftran, 2));
  assert(workspace.ensure(UnifiedGpuWorkspace::BufferKind::Btran, 2));
  assert(workspace.ensure(UnifiedGpuWorkspace::BufferKind::Pricing, 3));
  assert(workspace.ensure(UnifiedGpuWorkspace::BufferKind::Pivot, 2));
  assert(workspace.allocations() == 4);
  assert(workspace.reuses() == 4);
  assert(workspace.ftran_buffer()[0] == 7.0);
  assert(workspace.ftran_buffer()[1] == 9.0);

  // Growing only the pricing buffer adds one allocation; smaller later
  // requests reuse its capacity.
  assert(workspace.ensure(UnifiedGpuWorkspace::BufferKind::Pricing, 8));
  assert(workspace.allocations() == 5);
  assert(workspace.ensure(UnifiedGpuWorkspace::BufferKind::Pricing, 4));
  assert(workspace.reuses() == 5);

  const auto report = workspace.report();
  assert(report.persistent);
  assert(report.allocations == 5);
  assert(report.reuses == 5);
  assert(report.ftran_capacity >= 2);
  assert(report.pricing_capacity >= 8);

  workspace.release();
  assert(!workspace.initialized());
  assert(workspace.allocations() == 0);
  assert(workspace.reuses() == 0);

  std::cout << "Phase 5.2 unified persistent workspace: PASS\n";
  std::cout << "Allocations: " << report.allocations
            << " | Reuses: " << report.reuses
            << " | Persistent: YES\n";
  return 0;
}
