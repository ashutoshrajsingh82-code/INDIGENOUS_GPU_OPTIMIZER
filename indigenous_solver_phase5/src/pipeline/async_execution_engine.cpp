#include <chrono>

#include "pipeline/async_execution_engine.hpp"

namespace indigenous::pipeline {

AsyncExecutionEngine::~AsyncExecutionEngine() {
  wait_all();
}

AsyncExecutionEngine::TaskId AsyncExecutionEngine::submit(
    std::function<bool()> operation) {
  if (!operation) return 0;

  auto future = std::async(std::launch::async, std::move(operation)).share();

  std::lock_guard<std::mutex> lock(mutex_);
  const TaskId id = next_task_id_++;
  tasks_.emplace(id, TaskState{std::move(future)});
  ++submitted_;
  return id;
}

bool AsyncExecutionEngine::wait(TaskId task) {
  std::shared_future<bool> future;

  {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = tasks_.find(task);
    if (it == tasks_.end()) return false;
    future = it->second.future;
  }

  bool success = false;
  try {
    future.wait();
    success = future.get();
  } catch (...) {
    success = false;
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = tasks_.find(task);
    if (it == tasks_.end()) return success;
    tasks_.erase(it);
    ++completed_;
  }

  return success;
}

bool AsyncExecutionEngine::ready(TaskId task) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto it = tasks_.find(task);
  if (it == tasks_.end()) return false;

  return it->second.future.wait_for(std::chrono::seconds(0)) ==
         std::future_status::ready;
}

bool AsyncExecutionEngine::wait_all() {
  bool success = true;

  while (true) {
    TaskId task = 0;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (tasks_.empty()) break;
      task = tasks_.begin()->first;
    }

    if (!wait(task)) success = false;
  }

  return success;
}

AsyncExecutionEngine::Report AsyncExecutionEngine::report() const noexcept {
  std::lock_guard<std::mutex> lock(mutex_);

  Report result;
  result.submitted = submitted_;
  result.completed = completed_;
  result.in_flight = tasks_.size();
  return result;
}

void AsyncExecutionEngine::release() noexcept {
  wait_all();

  std::lock_guard<std::mutex> lock(mutex_);
  tasks_.clear();
  submitted_ = 0;
  completed_ = 0;
  next_task_id_ = 1;
}

}  // namespace indigenous::pipeline
