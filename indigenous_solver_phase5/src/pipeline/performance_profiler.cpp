#include "pipeline/performance_profiler.hpp"
#include <algorithm>
#include <chrono>

namespace indigenous::pipeline {
PerformanceProfiler::StageStats& PerformanceProfiler::at(StageStats* s, Stage stage) noexcept { return s[static_cast<int>(stage)]; }
const PerformanceProfiler::StageStats& PerformanceProfiler::at(const StageStats* s, Stage stage) noexcept { return s[static_cast<int>(stage)]; }
void PerformanceProfiler::record(Stage stage, double ms) noexcept {
  auto& s=at(stats_,stage); ++s.calls; s.total_ms+=ms;
  if(s.calls==1){s.minimum_ms=ms;s.maximum_ms=ms;}else{s.minimum_ms=std::min(s.minimum_ms,ms);s.maximum_ms=std::max(s.maximum_ms,ms);}
  s.average_ms=s.total_ms/static_cast<double>(s.calls);
}
void PerformanceProfiler::set_backend(const char* b) const noexcept { backend_=b?b:"UNKNOWN"; }
PerformanceProfiler::Report PerformanceProfiler::report() const noexcept {
  Report r; r.coordination=at(stats_,Stage::Coordination); r.btran=at(stats_,Stage::Btran);
  r.pricing=at(stats_,Stage::Pricing); r.ftran=at(stats_,Stage::Ftran);
  r.batch=at(stats_,Stage::Batch); r.async=at(stats_,Stage::Async); r.backend=backend_;
  r.total_ms=r.coordination.total_ms+r.batch.total_ms+r.async.total_ms;
  const StageStats* candidates[]={&r.coordination,&r.btran,&r.pricing,&r.ftran,&r.batch,&r.async};
  const Stage stages[]={Stage::Coordination,Stage::Btran,Stage::Pricing,Stage::Ftran,Stage::Batch,Stage::Async};
  double max_ms=0.0; Stage max_stage=Stage::Coordination;
  for(int i=0;i<6;++i){ if(candidates[i]->total_ms>max_ms){max_ms=candidates[i]->total_ms;max_stage=stages[i];} }
  r.bottleneck=stage_name(max_stage);
  const double denom=r.btran.total_ms+r.pricing.total_ms+r.ftran.total_ms+r.batch.total_ms+r.async.total_ms;
  r.bottleneck_percent=denom>0.0?(max_ms/denom)*100.0:0.0;
  return r;
}
const char* PerformanceProfiler::stage_name(Stage s) noexcept {
  switch(s){case Stage::Coordination:return "COORDINATION";case Stage::Btran:return "BTRAN";case Stage::Pricing:return "PRICING";case Stage::Ftran:return "FTRAN";case Stage::Batch:return "BATCH";case Stage::Async:return "ASYNC";} return "UNKNOWN";
}
ScopedPerformanceTimer::ScopedPerformanceTimer(PerformanceProfiler& p, PerformanceProfiler::Stage s) noexcept : profiler_(p),stage_(s),start_ns_(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()) {}
ScopedPerformanceTimer::~ScopedPerformanceTimer(){const auto now=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();profiler_.record(stage_,static_cast<double>(now-start_ns_)/1e6);}
}  // namespace indigenous::pipeline
