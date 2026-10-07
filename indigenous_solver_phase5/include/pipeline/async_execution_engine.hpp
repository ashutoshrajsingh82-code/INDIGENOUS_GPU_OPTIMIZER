#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <mutex>
#include <unordered_map>

namespace indigenous::pipeline {

// Phase 5.5 asynchronous task dispatcher.
//
// The dispatcher provides a backend-neutral async boundary around solver
// operations. On the current CPU-only machine it uses std::async with an
// explicit async launch policy. When the wrapped operation itself uses a CUDA
// backend, the same boundary allows the caller to return before the operation
// completes, while the operation remains responsible for its CUDA backend
// synchronization semantics.
class AsyncExecutionEngine final {
public:
  using TaskId = std::uint64_t;

  struct Report {
    std::size_t submitted = 0;
    std::size_t completed = 0;
    std::size_t in_flight = 0;
    bool asynchronous = true;
    bool gpu_capable = false;
    const char* backend = "CPU-ASYNC";
  };

  AsyncExecutionEngine() = default;
  ~AsyncExecutionEngine();

  AsyncExecutionEngine(const AsyncExecutionEngine&) = delete;
  AsyncExecutionEngine& operator=(const AsyncExecutionEngine&) = delete;

  TaskId submit(std::function<bool()> operation);

  // Waits for one task and consumes its completion state.
  bool wait(TaskId task);

  // Returns true only when a submitted task has completed.
  bool ready(TaskId task) const;

  // Waits for all currently submitted tasks.
  bool wait_all();

  Report report() const noexcept;

  void release() noexcept;

private:
  struct TaskState {
    std::shared_future<bool> future;
  };

  mutable std::mutex mutex_;
  std::unordered_map<TaskId, TaskState> tasks_;
  TaskId next_task_id_ = 1;
  std::size_t submitted_ = 0;
  std::size_t completed_ = 0;
};

}  // namespace indigenous::pipeline
