#include <iostream>
#include <string>

#include "pipeline/sparse_workload_planner.hpp"
#include "pipeline/unified_gpu_solver_pipeline.hpp"

namespace {
bool check(bool ok, const char* msg) {
  if (!ok) std::cerr << "FAIL: " << msg << "\n";
  return ok;
}
}

int main() {
  using indigenous::pipeline::SparseWorkloadPlanner;
  using indigenous::pipeline::UnifiedGpuSolverPipeline;

  SparseWorkloadPlanner planner({
      256ULL * 1024ULL * 1024ULL, 32, 256});

  const auto small = planner.plan({100, 200, 1000});
  if (!check(small.scale == SparseWorkloadPlanner::Scale::Small,
             "small classification")) return 1;

  const auto large = planner.plan({20000, 50000, 2'000'000});
  if (!check(large.scale == SparseWorkloadPlanner::Scale::Large,
             "large classification")) return 1;
  if (!check(large.large_scale, "large-scale flag")) return 1;
  if (!check(large.sparse, "sparse classification")) return 1;
  if (!check(large.recommended_batch_vectors >= 1 &&
             large.recommended_batch_vectors <= 256,
             "bounded batch size")) return 1;
  if (!check(large.chunk_columns >= 1 &&
             large.chunk_columns <= 50000,
             "bounded column chunk")) return 1;
  if (!check(large.estimated_csc_bytes > 0,
             "CSC memory estimate")) return 1;

  const auto huge = planner.plan({100000, 200000, 20'000'000});
  if (!check(huge.scale == SparseWorkloadPlanner::Scale::VeryLarge,
             "very-large classification")) return 1;
  if (!check(huge.large_scale, "very-large flag")) return 1;
  if (!check(huge.chunk_columns < 200000,
             "very-large chunking")) return 1;

  UnifiedGpuSolverPipeline::Options options;
  options.memory_budget_bytes = 64ULL * 1024ULL * 1024ULL;
  options.preferred_batch_vectors = 16;
  options.max_batch_vectors = 64;
  UnifiedGpuSolverPipeline pipeline(options);

  UnifiedGpuSolverPipeline::SparseColumns basis(2);
  basis[0] = {{0, 2.0}};
  basis[1] = {{0, 1.0}, {1, 1.0}};
  const std::vector<std::size_t> offsets{0, 1, 3};
  const std::vector<std::size_t> rows{0, 0, 1};
  const std::vector<double> values{2.0, 1.0, 1.0};
  const std::vector<double> objective{3.0, 4.0};

  if (!check(pipeline.initialize_basis(basis, 2), "pipeline basis")) return 1;
  if (!check(pipeline.initialize_pricing(offsets, rows, values, objective),
             "pipeline pricing")) return 1;

  const auto report = pipeline.report();
  if (!check(report.sparse_rows == 2 &&
             report.sparse_columns == 2 &&
             report.sparse_nonzeros == 3,
             "pipeline sparse metrics")) return 1;
  if (!check(report.estimated_csc_bytes > 0,
             "pipeline memory estimate")) return 1;
  if (!check(report.recommended_batch_vectors >= 1,
             "pipeline batch recommendation")) return 1;
  if (!check(report.chunk_columns >= 1,
             "pipeline chunk recommendation")) return 1;
  if (!check(report.sparse_strategy != nullptr,
             "pipeline strategy")) return 1;
  if (!check(!report.sparse_large_scale,
             "small pipeline remains non-large-scale")) return 1;

  const auto plan = pipeline.large_scale_plan();
  if (!check(report.sparse_nonzeros == 3, "exposed plan nonzeros")) return 1;

  std::cout << "Phase 5.7 large-scale sparse optimization: PASS\n";
  std::cout << "Planner: memory-aware sparse model planning | Strategy: "
            << report.sparse_strategy << "\n";
  std::cout << "Rows: " << report.sparse_rows
            << " | Columns: " << report.sparse_columns
            << " | NNZ: " << report.sparse_nonzeros << "\n";
  std::cout << "Batch recommendation: " << report.recommended_batch_vectors
            << " | Column chunk: " << report.chunk_columns << "\n";
  return 0;
}
