#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "pipeline/async_execution_engine.hpp"

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
  using indigenous::pipeline::AsyncExecutionEngine;

  AsyncExecutionEngine engine;

  const auto task = engine.submit([] {
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    return true;
  });

  if (!check(task != 0, "task submission")) return 1;

  const auto initial = engine.report();
  if (!check(initial.submitted == 1, "submitted count")) return 1;
  if (!check(initial.completed == 0, "initial completed count")) return 1;
  if (!check(initial.in_flight == 1, "initial in-flight count")) return 1;
  if (!check(initial.asynchronous, "asynchronous execution enabled")) return 1;
  if (!check(std::string(initial.backend) == "CPU-ASYNC",
             "CPU async backend report")) return 1;

  if (!check(engine.wait(task), "task completion")) return 1;

  const auto completed = engine.report();
  if (!check(completed.completed == 1, "completed count")) return 1;
  if (!check(completed.in_flight == 0, "no tasks in flight")) return 1;

  const auto second = engine.submit([] { return true; });
  const auto third = engine.submit([] { return true; });
  if (!check(second != 0 && third != 0, "multiple task submission")) return 1;
  if (!check(engine.wait_all(), "wait_all")) return 1;

  const auto final_report = engine.report();
  if (!check(final_report.submitted == 3, "total submitted count")) return 1;
  if (!check(final_report.completed == 3, "total completed count")) return 1;
  if (!check(final_report.in_flight == 0, "all tasks completed")) return 1;

  std::cout << "Phase 5.5 asynchronous execution engine: PASS\n";
  std::cout << "Submitted: " << final_report.submitted
            << " | Completed: " << final_report.completed
            << " | In-flight: " << final_report.in_flight
            << " | Backend: " << final_report.backend << "\n";
  return 0;
}
