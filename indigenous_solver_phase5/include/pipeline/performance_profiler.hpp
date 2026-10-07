#pragma once
#include <cstddef>
#include <string>

namespace indigenous::pipeline {

class PerformanceProfiler final {
public:
  enum class Stage { Coordination, Btran, Pricing, Ftran, Batch, Async };

  struct StageStats {
    std::size_t calls = 0;
    double total_ms = 0.0;
    double average_ms = 0.0;
    double minimum_ms = 0.0;
    double maximum_ms = 0.0;
  };

  struct Report {
    StageStats coordination;
    StageStats btran;
    StageStats pricing;
    StageStats ftran;
    StageStats batch;
    StageStats async;
    double total_ms = 0.0;
    const char* bottleneck = "NONE";
    double bottleneck_percent = 0.0;
    const char* backend = "CPU";
  };

  void record(Stage stage, double elapsed_ms) noexcept;
  Report report() const noexcept;
  void set_backend(const char* backend) const noexcept;
  static const char* stage_name(Stage stage) noexcept;
private:
  StageStats stats_[6]{};
  mutable const char* backend_ = "CPU";
  static StageStats& at(StageStats* stats, Stage stage) noexcept;
  static const StageStats& at(const StageStats* stats, Stage stage) noexcept;
};

class ScopedPerformanceTimer final {
public:
  ScopedPerformanceTimer(PerformanceProfiler& profiler, PerformanceProfiler::Stage stage) noexcept;
  ~ScopedPerformanceTimer();
  ScopedPerformanceTimer(const ScopedPerformanceTimer&) = delete;
  ScopedPerformanceTimer& operator=(const ScopedPerformanceTimer&) = delete;
private:
  PerformanceProfiler& profiler_;
  PerformanceProfiler::Stage stage_;
  long long start_ns_;
};

}  // namespace indigenous::pipeline
