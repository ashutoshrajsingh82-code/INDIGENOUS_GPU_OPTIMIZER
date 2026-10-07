#include <iostream>
#include "pipeline/performance_profiler.hpp"
int main(){
  indigenous::pipeline::PerformanceProfiler p; p.set_backend("CPU");
  p.record(indigenous::pipeline::PerformanceProfiler::Stage::Btran,2.0);
  p.record(indigenous::pipeline::PerformanceProfiler::Stage::Pricing,5.0);
  p.record(indigenous::pipeline::PerformanceProfiler::Stage::Ftran,3.0);
  const auto r=p.report();
  if(r.bottleneck!=std::string("PRICING")||r.pricing.calls!=1||r.pricing.total_ms!=5.0)return 1;
  std::cout<<"Phase 5.10 performance profiler: PASS\n";
  std::cout<<"BTRAN: "<<r.btran.total_ms<<" ms | Pricing: "<<r.pricing.total_ms<<" ms | FTRAN: "<<r.ftran.total_ms<<" ms\n";
  std::cout<<"Bottleneck: "<<r.bottleneck<<" ("<<r.bottleneck_percent<<"%) | Backend: "<<r.backend<<"\n";
  return 0;
}
