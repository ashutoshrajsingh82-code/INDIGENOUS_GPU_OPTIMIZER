#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cctype>
#include <cmath>
#include <iostream>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "pipeline/production_solver.hpp"
#include "solver/io/lp_reader.hpp"
#include "solver/io/mps_reader.hpp"
#include "solver/model/solution_validator.hpp"

namespace fs = std::filesystem;
using indigenous::pipeline::ProductionSolver;
using solver::LinearModel;
using solver::SolveResult;

namespace {

struct Job {
  std::string id;
  std::string model_id;
  std::string model_path;
  std::string status = "queued";
  SolveResult result{};
  ProductionSolver::Report report{};
  LinearModel model{};
  solver::ValidationReport certificate{};
  std::chrono::steady_clock::time_point started{};
  double elapsed_ms = 0.0;
};

std::mutex g_mutex;
std::unordered_map<std::string, Job> g_jobs;
std::unordered_map<std::string, std::string> g_models;
std::atomic<unsigned long long> g_sequence{1};

std::string json_escape(const std::string& value) {
  std::ostringstream out;
  for (unsigned char c : value) {
    switch (c) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\b': out << "\\b"; break;
      case '\f': out << "\\f"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (c < 0x20) {
          out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
              << static_cast<int>(c) << std::dec;
        } else out << static_cast<char>(c);
    }
  }
  return out.str();
}

std::string js(const std::string& value) { return "\"" + json_escape(value) + "\""; }
std::string jb(bool v) { return v ? "true" : "false"; }

std::string jn(double v) {
  if (!std::isfinite(v)) return "null";
  std::ostringstream s;
  s << std::setprecision(15) << v;
  return s.str();
}

std::string ju(std::size_t v) { return std::to_string(v); }

// Forward declaration used by benchmark/report JSON builders below.
std::string now_utc_iso();

std::string status_string(solver::SolveStatus s) {
  switch (s) {
    case solver::SolveStatus::Optimal: return "optimal";
    case solver::SolveStatus::Infeasible: return "infeasible";
    case solver::SolveStatus::Unbounded: return "unbounded";
    case solver::SolveStatus::IterationLimit: return "iteration_limit";
    default: return "error";
  }
}
std::string status_message(solver::SolveStatus s) {
  return solver::to_string(s);
}

bool load_model(const std::string& path, LinearModel& model, std::string& error) {
  const auto p = path.find_last_of('.');
  const auto ext = p == std::string::npos ? "" : path.substr(p + 1);
  if (ext == "mps" || ext == "MPS") return solver::read_mps(path, model, error);
  return solver::read_lp(path, model, error);
}

std::optional<std::string> json_string_field(const std::string& body, const std::string& key) {
  const auto k = body.find("\"" + key + "\"");
  if (k == std::string::npos) return std::nullopt;
  const auto colon = body.find(':', k);
  if (colon == std::string::npos) return std::nullopt;
  const auto q1 = body.find('"', colon + 1);
  if (q1 == std::string::npos) return std::nullopt;
  const auto q2 = body.find('"', q1 + 1);
  if (q2 == std::string::npos) return std::nullopt;
  return body.substr(q1 + 1, q2 - q1 - 1);
}
std::optional<std::size_t> json_size_field(const std::string& body, const std::string& key) {
  const auto k = body.find("\"" + key + "\"");
  if (k == std::string::npos) return std::nullopt;
  const auto colon = body.find(':', k);
  if (colon == std::string::npos) return std::nullopt;
  std::size_t end = colon + 1;
  while (end < body.size() && std::isspace(static_cast<unsigned char>(body[end]))) ++end;
  const auto first = end;
  while (end < body.size() && std::isdigit(static_cast<unsigned char>(body[end]))) ++end;
  if (first == end) return std::nullopt;
  try { return static_cast<std::size_t>(std::stoull(body.substr(first, end - first))); }
  catch (...) { return std::nullopt; }
}

std::string extension_format(const std::string& path) {
  const auto p = path.find_last_of('.');
  if (p == std::string::npos) return "LP";
  auto ext = path.substr(p + 1);
  for (char& c : ext) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return ext == "MPS" ? "MPS" : "LP";
}

std::string make_id(const char* prefix) {
  return std::string(prefix) + "-" + std::to_string(g_sequence.fetch_add(1));
}


struct BenchmarkCliResult {
  bool available = false;
  int exit_code = -1;
  std::string output;
};

BenchmarkCliResult run_benchmark_cli(const fs::path& executable,
                                     const fs::path& model_path) {
  BenchmarkCliResult result;

  if (!fs::exists(executable) || !fs::exists(model_path)) {
    return result;
  }

  const std::string command =
      "\"" + executable.string() + "\" solve \"" +
      model_path.string() + "\" 2>&1";

  FILE* pipe = _popen(command.c_str(), "r");
  if (!pipe) {
    return result;
  }

  char buffer[4096];
  while (std::fgets(buffer, sizeof(buffer), pipe)) {
    result.output += buffer;
  }

  result.exit_code = _pclose(pipe);
  result.available = (result.exit_code == 0);
  return result;
}

std::optional<std::string> benchmark_metric(
    const std::string& text,
    const std::string& key) {
  const std::string needle = key + ":";
  const auto pos = text.find(needle);
  if (pos == std::string::npos) {
    return std::nullopt;
  }

  const auto start = pos + needle.size();
  auto end = text.find('\n', start);
  if (end == std::string::npos) {
    end = text.size();
  }

  auto value = text.substr(start, end - start);
  while (!value.empty() &&
         (value.back() == '\r' || value.back() == ' ' ||
          value.back() == '\t')) {
    value.pop_back();
  }

  const auto first = value.find_first_not_of(" \t");
  if (first == std::string::npos) {
    return std::nullopt;
  }

  return value.substr(first);
}

std::optional<double> benchmark_double(
    const std::string& text,
    const std::string& key) {
  const auto value = benchmark_metric(text, key);
  if (!value) {
    return std::nullopt;
  }

  try {
    return std::stod(*value);
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<int> benchmark_int(
    const std::string& text,
    const std::string& key) {
  const auto value = benchmark_metric(text, key);
  if (!value) {
    return std::nullopt;
  }

  try {
    return std::stoi(*value);
  } catch (...) {
    return std::nullopt;
  }
}

std::string benchmark_status(const std::string& text) {
  const auto value = benchmark_metric(text, "Status");
  if (!value) {
    return "error";
  }

  std::string status = *value;
  std::transform(status.begin(), status.end(), status.begin(),
                 [](unsigned char c) {
                   return static_cast<char>(std::tolower(c));
                 });

  if (status.find("optimal") != std::string::npos) return "optimal";
  if (status.find("infeasible") != std::string::npos) return "infeasible";
  if (status.find("unbounded") != std::string::npos) return "unbounded";
  if (status.find("iteration") != std::string::npos) return "iteration_limit";
  return "error";
}

std::string benchmark_backend(const std::string& text) {
  const auto value = benchmark_metric(text, "Pricing backend");
  if (!value) {
    return "UNKNOWN";
  }

  if (value->find("CUDA") != std::string::npos ||
      value->find("GPU") != std::string::npos) {
    return "CUDA";
  }

  if (value->find("CPU") != std::string::npos) {
    return "CPU";
  }

  return "UNKNOWN";
}

std::string benchmark_case_json(
    const std::string& model,
    const std::optional<double>& phase2_ms,
    const std::optional<double>& phase3_ms,
    const std::optional<double>& phase4_ms,
    const std::string& backend,
    const std::string& status,
    bool certificate_passed,
    const std::optional<int>& iterations,
    const std::optional<double>& objective,
    const std::string& message) {
  std::ostringstream out;
  out << "{";
  out << "\"model\":" << js(model);
  out << ",\"phase2Ms\":" << (phase2_ms ? jn(*phase2_ms) : "null");
  out << ",\"phase3Ms\":" << (phase3_ms ? jn(*phase3_ms) : "null");
  out << ",\"phase4Ms\":" << (phase4_ms ? jn(*phase4_ms) : "null");

  if (phase2_ms && phase3_ms && *phase3_ms > 0.0) {
    out << ",\"speedup\":" << jn(*phase2_ms / *phase3_ms);
  } else {
    out << ",\"speedup\":null";
  }

  out << ",\"backend\":" << js(backend);
  out << ",\"status\":" << js(status);
  out << ",\"certificatePassed\":" << jb(certificate_passed);
  out << ",\"iterations\":"
      << (iterations ? std::to_string(*iterations) : "null");
  out << ",\"objective\":"
      << (objective ? jn(*objective) : "null");
  out << ",\"message\":" << js(message);
  out << "}";
  return out.str();
}

std::string benchmark_snapshot_json() {
  /*
   * The benchmark endpoint executes the same Phase 2 and Phase 3
   * solver CLIs used by the existing benchmark scripts.
   *
   * Phase 4 is intentionally null: Phase 4's benchmark status is a
   * validated basis backend, not a full revised-simplex replacement.
   * No synthetic Phase 4 timing is generated.
   */
  // Resolve the benchmark data root independently of the process working directory.
  // The current repository has Phase 6 at the outer root while Phase 2 and
  // the Netlib models are stored in the nested INDIGENOUS_GPU_OPTIMIZER tree.
  fs::path outer_root = fs::current_path();

  for (int depth = 0; depth < 8; ++depth) {
    const fs::path nested_repo =
        outer_root / "INDIGENOUS_GPU_OPTIMIZER";

    if (fs::exists(outer_root / "benchmarks" / "netlib") ||
        fs::exists(nested_repo / "benchmarks" / "netlib")) {
      break;
    }

    const fs::path parent = outer_root.parent_path();
    if (parent == outer_root) {
      break;
    }

    outer_root = parent;
  }

  fs::path data_root = outer_root;

  if (!fs::exists(data_root / "benchmarks" / "netlib")) {
    const fs::path nested_repo =
        outer_root / "INDIGENOUS_GPU_OPTIMIZER";

    if (fs::exists(nested_repo / "benchmarks" / "netlib")) {
      data_root = nested_repo;
    }
  }

  const fs::path models_path =
      data_root / "benchmarks" / "netlib";

  const fs::path phase2_exe =
      data_root / "indigenous_solver_phase2" /
      "build" / "Release" /
      "solver_phase2_cli.exe";

  fs::path phase3_exe =
      data_root / "indigenous_solver_phase3" /
      "build" / "Release" /
      "solver_phase3_cli.exe";

  // In the current Phase 6 build tree, Phase 3 is embedded under Phase 5.
  if (!fs::exists(phase3_exe)) {
    phase3_exe =
        outer_root / "indigenous_solver_phase6" /
        "build" / "phase5_build" /
        "phase3_build" / "Release" /
        "solver_phase3_cli.exe";
  }

  const std::vector<std::string> models = {
      "afiro", "adlittle", "blend", "bore3d", "brandy",
      "grow15", "kb2", "lotfi", "sc50b", "share1b"};

  std::ostringstream cases;
  std::size_t model_count = 0;
  std::size_t phase3_faster = 0;
  std::size_t phase4_faster = 0;
  std::size_t certificate_pass = 0;
  std::size_t cpu_runs = 0;
  std::size_t gpu_runs = 0;
  double phase2_total = 0.0;
  double phase3_total = 0.0;
  bool any_case = false;
  bool all_case_data_valid = true;

  for (const auto& model_name : models) {
    const fs::path model_path =
        models_path / (model_name + ".mps");

    const auto p2 =
        run_benchmark_cli(phase2_exe, model_path);

    const auto p3 =
        run_benchmark_cli(phase3_exe, model_path);

    const auto p2_ms =
        benchmark_double(p2.output, "Timing total_ms");
    const auto p3_ms =
        benchmark_double(p3.output, "Timing total_ms");

    const auto p3_backend =
        benchmark_backend(p3.output);

    const auto p3_status =
        benchmark_status(p3.output);

    const auto p3_certificate =
        benchmark_metric(p3.output, "Certificate");

    const auto p3_iterations =
        benchmark_int(p3.output, "Iterations");

    const auto p3_objective =
        benchmark_double(p3.output, "Objective");

    if (!p2.available || !p3.available ||
        !p2_ms || !p3_ms) {
      all_case_data_valid = false;
    }

    if (p2_ms) phase2_total += *p2_ms;
    if (p3_ms) phase3_total += *p3_ms;

    if (p2_ms && p3_ms) {
      if (*p3_ms < *p2_ms) {
        ++phase3_faster;
      }
    }

    const bool cert =
        p3_certificate &&
        *p3_certificate == "PASS";

    if (cert) {
      ++certificate_pass;
    }

    if (p3_backend == "CUDA") {
      ++gpu_runs;
    } else if (p3_backend == "CPU") {
      ++cpu_runs;
    }

    if (model_count > 0) {
      cases << ",";
    }

    const std::string message =
        !p3.available
            ? "Phase 3 benchmark executable or model is unavailable."
            : (!p3_ms
                   ? "Phase 3 timing was not present in solver output."
                   : "Measured from the Phase 3 solver CLI.");

    cases << benchmark_case_json(
        model_name,
        p2_ms,
        p3_ms,
        std::nullopt,
        p3_backend,
        p3_status,
        cert,
        p3_iterations,
        p3_objective,
        message);

    ++model_count;
    any_case = true;
  }

  std::ostringstream out;

  if (!any_case ||
      !fs::exists(phase2_exe) ||
      !fs::exists(phase3_exe) ||
      !fs::exists(models_path)) {
    out << "{"
        << "\"summary\":{"
        << "\"status\":\"unavailable\","
        << "\"modelCount\":0,"
        << "\"phase2TotalMs\":null,"
        << "\"phase3TotalMs\":null,"
        << "\"phase4TotalMs\":null,"
        << "\"aggregateSpeedup\":null,"
        << "\"phase3FasterCount\":0,"
        << "\"phase4FasterCount\":0,"
        << "\"certificatePassCount\":0,"
        << "\"gpuRuns\":0,"
        << "\"cpuRuns\":0,"
        << "\"timestamp\":" << js(now_utc_iso()) << ","
        << "\"message\":"
        << js("Benchmark executables or Netlib models are not available. "
              "Build Phase 2 and Phase 3 and keep benchmarks/netlib "
              "available to expose benchmark data.")
        << "},"
        << "\"cases\":[]}";
    return out.str();
  }

  const double aggregate =
      phase3_total > 0.0
          ? phase2_total / phase3_total
          : 0.0;

  std::string message =
      "Measured from the Phase 2 and Phase 3 solver CLIs on the "
      "local Netlib benchmark set. Phase 4 timing is unavailable "
      "because Phase 4 is a validated basis backend rather than "
      "a full revised-simplex replacement.";

  if (!all_case_data_valid) {
    message +=
        " One or more benchmark cases did not return complete timing data.";
  }

  out << "{"
      << "\"summary\":{"
      << "\"status\":\"ok\","
      << "\"modelCount\":" << model_count << ","
      << "\"phase2TotalMs\":" << jn(phase2_total) << ","
      << "\"phase3TotalMs\":" << jn(phase3_total) << ","
      << "\"phase4TotalMs\":null,"
      << "\"aggregateSpeedup\":" << jn(aggregate) << ","
      << "\"phase3FasterCount\":" << phase3_faster << ","
      << "\"phase4FasterCount\":" << phase4_faster << ","
      << "\"certificatePassCount\":" << certificate_pass << ","
      << "\"gpuRuns\":" << gpu_runs << ","
      << "\"cpuRuns\":" << cpu_runs << ","
      << "\"timestamp\":" << js(now_utc_iso()) << ","
      << "\"message\":" << js(message)
      << "},"
      << "\"cases\":[" << cases.str() << "]}";

  return out.str();
}

std::string report_runtime(const ProductionSolver::Report& r) {
  return "{"
    "\"status\":\"ok\","
    "\"executionBackend\":" + js(r.execution_backend ? r.execution_backend : "CPU") + ","
    "\"cudaCompiled\":false,\"cudaDeviceReady\":false,\"gpuRuntimeActive\":" + jb(r.gpu_active) + ","
    "\"basisGpuActive\":false,\"pricingGpuActive\":" + jb(std::string(r.pipeline_pricing_backend ? r.pipeline_pricing_backend : "CPU") == "CUDA") + ","
    "\"ftranCalls\":0,\"btranCalls\":0,"
    "\"pricingCalls\":" + ju(r.pipeline_numerical_checks) + ",\"updateCalls\":0,"
    "\"workspaceAllocations\":0,\"workspaceReuses\":0,\"workspacePersistent\":true,"
    "\"asyncBackend\":\"CPU\",\"batchBackend\":\"CPU-BATCH\",\"adaptiveGpuEligible\":false,"
    "\"numericalStable\":" + jb(r.pipeline_numerical_failures == 0) + ","
    "\"numericalChecks\":" + ju(r.pipeline_numerical_checks) + ","
    "\"numericalFailures\":" + ju(r.pipeline_numerical_failures) + ","
    "\"fallbackCount\":0,\"bottleneck\":\"PRICING\",\"bottleneckShare\":0,"
    "\"version\":\"phase6-api-0.1.0\","
    "\"message\":" + js(r.message) + "}";
}

std::string result_json(const Job& job) {
  const auto& m = job.model;
  const auto& r = job.result;
  const auto& c = job.certificate;
  std::ostringstream out;
  out << "{";
  out << "\"jobId\":" << js(job.id) << ",\"modelId\":" << js(job.model_id);
  out << ",\"status\":" << js(status_string(r.status));
  out << ",\"objective\":" << jn(r.objective_value);
  out << ",\"iterations\":" << ju(r.iterations);
  out << ",\"elapsedMs\":" << jn(job.elapsed_ms);
  out << ",\"variables\":[";
  for (std::size_t i=0;i<m.variables.size();++i) {
    if (i) out << ",";
    const double x = i < r.primal.size() ? r.primal[i] : 0.0;
    out << "{\"name\":" << js(m.variables[i].name) << ",\"value\":" << jn(x)
        << ",\"reducedCost\":null,\"lowerBound\":" << jn(m.variables[i].lower_bound)
        << ",\"upperBound\":" << jn(m.variables[i].upper_bound) << "}";
  }
  out << "],\"constraints\":[";
  const auto& A = m.A;
  for (std::size_t i=0;i<m.constraints.size();++i) {
    if (i) out << ",";
    double activity = 0.0;
    for (std::size_t j=0;j<m.variables.size();++j) {
      const auto& cp=A.column_pointers();
      for (auto k=cp[j]; k<cp[j+1]; ++k)
        if (static_cast<std::size_t>(A.row_indices()[k]) == i)
          activity += A.values()[k] * (j < r.primal.size() ? r.primal[j] : 0.0);
    }
    const double rhs = std::isfinite(m.constraints[i].upper_bound)
      ? m.constraints[i].upper_bound : m.constraints[i].lower_bound;
    const double dual = i < r.dual.size() ? r.dual[i] : 0.0;
    out << "{\"name\":" << js(m.constraints[i].name) << ",\"activity\":" << jn(activity)
        << ",\"rhs\":" << jn(rhs) << ",\"dualValue\":" << jn(dual)
        << ",\"residual\":null}";
  }
  out << "],\"certificate\":{"
      << "\"passed\":" << jb(c.valid)
      << ",\"primalResidual\":" << jn(r.primal_residual)
      << ",\"dualResidual\":" << jn(r.dual_residual)
      << ",\"complementarityResidual\":null"
      << ",\"message\":" << js(c.message) << "}";
  out << ",\"backend\":" << js(r.statistics.pricing_backend == "CUDA" ? "CUDA" : "CPU")
      << ",\"gpuActive\":" << jb(job.report.gpu_active)
      << ",\"message\":" << js(r.message) << "}";
  return out.str();
}

std::string now_utc_iso() {
  const auto now = std::chrono::system_clock::now();
  const auto tt = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
#ifdef _WIN32
  gmtime_s(&tm, &tt);
#else
  gmtime_r(&tt, &tm);
#endif
  std::ostringstream out;
  out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  return out.str();
}

std::string report_json(const Job& job) {
  std::ostringstream out;
  out << "{"
      << "\"status\":\"ok\",\"report\":{"
      << "\"reportId\":" << js("report-" + job.id)
      << ",\"generatedAt\":" << js(now_utc_iso())
      << ",\"jobId\":" << js(job.id)
      << ",\"model\":{\"name\":" << js(job.model.name)
      << ",\"modelId\":" << js(job.model_id)
      << ",\"format\":" << js(extension_format(job.model_path))
      << ",\"rows\":" << ju(job.model.constraints.size())
      << ",\"columns\":" << ju(job.model.variables.size())
      << ",\"nonzeros\":" << ju(job.model.A.values().size()) << "}"
      << ",\"result\":" << result_json(job)
      << ",\"runtime\":" << report_runtime(job.report)
      << ",\"benchmark\":null"
      << ",\"verification\":{"
      << "\"summary\":{\"status\":" << js(job.certificate.valid ? "ok" : "failed")
      << ",\"passed\":" << (job.certificate.valid ? "1" : "0")
      << ",\"total\":1,\"failed\":" << (job.certificate.valid ? "0" : "1")
      << ",\"skipped\":0,\"numericalStable\":" << jb(job.report.pipeline_numerical_failures == 0)
      << ",\"certificatePassCount\":" << (job.certificate.valid ? "1" : "0")
      << ",\"fallbackChecksPassed\":null,\"regressionChecksPassed\":null,\"ftranChecksPassed\":null"
      << ",\"btranChecksPassed\":null,\"pricingChecksPassed\":null,\"workspaceChecksPassed\":null"
      << ",\"gpuRuntimeReady\":false,\"lastVerifiedAt\":" << js(now_utc_iso()) << "},"
      << "\"checks\":[{\"id\":\"solution-certificate\",\"category\":\"numerical\",\"name\":\"Solution certificate\","
      << "\"status\":" << js(job.certificate.valid ? "passed" : "failed")
      << ",\"severity\":" << js(job.certificate.valid ? "info" : "error")
      << ",\"message\":" << js(job.certificate.message) << "}]}"
      << ",\"architecture\":null"
      << "},\"message\":\"Authoritative Phase 5 production solver report.\"}";
  return out.str();
}

struct Request {
  std::string method, target, headers, body;
};

bool recv_request(SOCKET s, Request& req) {
  std::string data;
  char buffer[8192];
  std::size_t header_end = std::string::npos;
  std::size_t content_length = 0;
  for (;;) {
    const int n=recv(s,buffer,sizeof(buffer),0);
    if(n<=0) return false;
    data.append(buffer,n);
    if(header_end==std::string::npos) {
      header_end=data.find("\r\n\r\n");
      if(header_end!=std::string::npos) {
        std::istringstream h(data.substr(0,header_end));
        h >> req.method >> req.target;
        std::string line;
        std::getline(h,line);
        while(std::getline(h,line)) {
          if(line.rfind("Content-Length:",0)==0) content_length=std::stoull(line.substr(15));
        }
        header_end += 4;
      }
    }
    if(header_end!=std::string::npos && data.size() >= header_end + content_length) break;
    if(data.size()>16*1024*1024) return false;
  }
  req.headers=data.substr(0,header_end);
  req.body=data.substr(header_end,content_length);
  return true;
}

std::string header_value(const std::string& headers, const std::string& name) {
  std::istringstream s(headers); std::string line;
  while(std::getline(s,line)) {
    if(!line.empty() && line.back()=='\r') line.pop_back();
    if(line.rfind(name,0)==0) return line.substr(name.size());
  }
  return {};
}

void send_response(SOCKET s, int code, const std::string& body, const char* type="application/json") {
  const char* text = code==200 ? "OK" : code==201 ? "Created" : code==404 ? "Not Found" : code==400 ? "Bad Request" : "Internal Server Error";
  std::ostringstream h;
  h << "HTTP/1.1 " << code << " " << text << "\r\nContent-Type: " << type
    << "\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: GET,POST,OPTIONS\r\n"
    << "Access-Control-Allow-Headers: Content-Type\r\nContent-Length: " << body.size()
    << "\r\nConnection: close\r\n\r\n";
  send(s,h.str().c_str(),static_cast<int>(h.str().size()),0);
  if(!body.empty()) send(s,body.data(),static_cast<int>(body.size()),0);
}

std::string multipart_model(const Request& req, std::string& filename, std::string& error) {
  const auto ct=header_value(req.headers,"Content-Type:");
  const auto bp=ct.find("boundary=");
  if(bp==std::string::npos){error="multipart boundary missing";return{};}
  const std::string boundary="--"+ct.substr(bp+9);
  const auto fp=req.body.find("filename=\"");
  if(fp==std::string::npos){error="multipart filename missing";return{};}
  const auto fe=req.body.find('"',fp+10);
  filename=req.body.substr(fp+10,fe-(fp+10));
  filename=fs::path(filename).filename().string();
  const auto hs=req.body.find("\r\n\r\n",fe);
  if(hs==std::string::npos){error="multipart header missing";return{};}
  const auto start=hs+4;
  auto end=req.body.find("\r\n"+boundary,start);
  if(end==std::string::npos) end=req.body.size();
  return req.body.substr(start,end-start);
}

void handle(SOCKET s) {
  Request req;
  if(!recv_request(s,req)){closesocket(s);return;}
  if(req.method=="OPTIONS"){send_response(s,200,"{}");closesocket(s);return;}

  try {
    if(req.method=="GET" && req.target=="/health") {
      send_response(s,200,R"({"status":"ok","executionBackend":"CPU","cudaCompiled":false,"cudaDeviceReady":false,"cpuFallbackEnabled":true,"version":"phase6-api-0.1.0"})");
    } else if(req.method=="POST" && req.target=="/models/inspect") {
      std::string filename,error; const auto bytes=multipart_model(req,filename,error);
      if(!error.empty()){send_response(s,400,"{\"message\":"+js(error)+"}");closesocket(s);return;}
      const fs::path dir=fs::temp_directory_path()/"indigenous_solver_phase6";
      fs::create_directories(dir);
      const std::string id=make_id("model");
      const fs::path path=dir/(id+"_"+filename);
      std::ofstream f(path,std::ios::binary); f.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));
      LinearModel model;
      if(!load_model(path.string(),model,error)){send_response(s,400,"{\"message\":"+js(error)+"}");closesocket(s);return;}
      {std::lock_guard<std::mutex> lock(g_mutex);g_models[id]=path.string();}
      std::ostringstream out;
      out<<"{\"modelId\":"<<js(id)<<",\"name\":"<<js(model.name)
         <<",\"format\":"<<js(extension_format(filename))
         <<",\"sizeBytes\":"<<ju(bytes.size())<<",\"status\":\"Ready\",\"message\":\"Model parsed successfully.\",\"rows\":"<<ju(model.constraints.size())<<",\"columns\":"<<ju(model.variables.size())
         <<",\"nonzeros\":"<<ju(model.A.values().size())<<",\"valid\":true}";
      send_response(s,200,out.str());
    } else if(req.method=="POST" && req.target=="/solve") {
      const auto mid=req.body.find("\"modelId\"");
      if(mid==std::string::npos){send_response(s,400,R"({"message":"modelId is required"})");closesocket(s);return;}
      const auto q=req.body.find('"',req.body.find(':',mid)+1);
      const auto e=req.body.find('"',q+1);
      const std::string model_id=req.body.substr(q+1,e-q-1);
      std::string path; {std::lock_guard<std::mutex> lock(g_mutex);auto it=g_models.find(model_id);if(it!=g_models.end())path=it->second;}
      if(path.empty()){send_response(s,404,R"({"message":"unknown modelId"})");closesocket(s);return;}
      Job job; job.id=make_id("job"); job.model_id=model_id; job.model_path=path; job.started=std::chrono::steady_clock::now();
      std::string error; if(!load_model(path,job.model,error)){send_response(s,400,"{\"message\":"+js(error)+"}");closesocket(s);return;}
      ProductionSolver::Options options;
      if (const auto max_it = json_size_field(req.body, "maxIterations"))
        options.simplex.max_iterations = *max_it;
      const auto backend_policy = json_string_field(req.body, "backendPolicy");
      if (backend_policy && *backend_policy == "cuda") {
        send_response(s,400,R"({"message":"CUDA backend was requested, but this build has no CUDA runtime/device."})");
        closesocket(s); return;
      }
      ProductionSolver production(options);
      job.result=production.solve(job.model); job.report=production.report();
      if(!job.result.primal.empty()) job.certificate=solver::validate_solution(job.model,job.result.primal,job.result.objective_value);
      job.elapsed_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-job.started).count();
      job.status=status_string(job.result.status);
      {std::lock_guard<std::mutex> lock(g_mutex);g_jobs[job.id]=job;}
      send_response(s,200,"{\"jobId\":"+js(job.id)+",\"modelId\":"+js(model_id)+",\"status\":"+js(job.status)+",\"message\":"+js(job.result.message)+",\"progress\":100,\"iteration\":"+ju(job.result.iterations)+",\"objective\":"+jn(job.result.objective_value)+",\"elapsedMs\":"+jn(job.elapsed_ms)+"}");
    } else if(req.method=="GET") {
      const std::string prefix="/solve/";
      if(req.target.rfind(prefix,0)==0) {
        std::string rest=req.target.substr(prefix.size());
        bool result=false; if(rest.size()>7 && rest.substr(rest.size()-7)=="/result"){result=true;rest.resize(rest.size()-7);}
        std::lock_guard<std::mutex> lock(g_mutex); auto it=g_jobs.find(rest);
        if(it==g_jobs.end()){send_response(s,404,R"({"message":"unknown jobId"})");}
        else if(result) send_response(s,200,result_json(it->second));
        else send_response(s,200,"{\"jobId\":"+js(it->second.id)+",\"modelId\":"+js(it->second.model_id)+",\"status\":"+js(it->second.status)+",\"message\":"+js(it->second.result.message)+",\"progress\":100,\"iteration\":"+ju(it->second.result.iterations)+",\"objective\":"+jn(it->second.result.objective_value)+",\"elapsedMs\":"+jn(it->second.elapsed_ms)+"}");
      } else if(req.target=="/runtime") {
        std::lock_guard<std::mutex> lock(g_mutex);
        if(g_jobs.empty()) send_response(s,200,R"({"status":"ok","executionBackend":"CPU","cudaCompiled":false,"cudaDeviceReady":false,"gpuRuntimeActive":false,"basisGpuActive":false,"pricingGpuActive":false,"ftranCalls":0,"btranCalls":0,"pricingCalls":0,"updateCalls":0,"workspaceAllocations":0,"workspaceReuses":0,"workspacePersistent":true,"asyncBackend":"CPU","batchBackend":"CPU-BATCH","adaptiveGpuEligible":false,"numericalStable":true,"numericalChecks":0,"numericalFailures":0,"fallbackCount":0,"bottleneck":"PRICING","bottleneckShare":0,"version":"phase6-api-0.1.0","message":"No solve has run yet."})");
        else send_response(s,200,report_runtime(g_jobs.begin()->second.report));
      } else if(req.target=="/verification") {
        std::lock_guard<std::mutex> lock(g_mutex);
        if(g_jobs.empty()) send_response(s,200,R"({"status":"unavailable","summary":{"status":"unavailable","passed":0,"total":0,"failed":0,"skipped":0,"numericalStable":null,"certificatePassCount":null,"fallbackChecksPassed":null,"regressionChecksPassed":null,"ftranChecksPassed":null,"btranChecksPassed":null,"pricingChecksPassed":null,"workspaceChecksPassed":null,"gpuRuntimeReady":false,"message":"No completed solve is available."},"checks":[]})");
        else {
          const auto& j=g_jobs.begin()->second;
          const std::string st=j.certificate.valid?"ok":"failed";
          send_response(s,200,"{\"status\":"+js(st)+",\"summary\":{\"status\":"+js(st)+",\"passed\":"+std::string(j.certificate.valid?"1":"0")+",\"total\":1,\"failed\":"+std::string(j.certificate.valid?"0":"1")+",\"skipped\":0,\"numericalStable\":"+jb(j.report.pipeline_numerical_failures==0)+",\"certificatePassCount\":"+std::string(j.certificate.valid?"1":"0")+",\"fallbackChecksPassed\":null,\"regressionChecksPassed\":null,\"ftranChecksPassed\":null,\"btranChecksPassed\":null,\"pricingChecksPassed\":null,\"workspaceChecksPassed\":null,\"gpuRuntimeReady\":false},\"checks\":[]}");
        }
      } else if(req.target=="/benchmarks") {
        send_response(s,200,benchmark_snapshot_json());
      } else if(req.target=="/reports/latest") {
        std::lock_guard<std::mutex> lock(g_mutex);
        if(g_jobs.empty()) send_response(s,200,R"({"status":"unavailable","report":null,"message":"No completed solve is available for reporting."})");
        else send_response(s,200,report_json(g_jobs.begin()->second));
      } else if(req.target.rfind("/reports/",0)==0) {
        const std::string id=req.target.substr(9); std::lock_guard<std::mutex> lock(g_mutex); auto it=g_jobs.find(id);
        if(it==g_jobs.end()) send_response(s,404,R"({"status":"unavailable","report":null,"message":"Unknown jobId."})");
        else send_response(s,200,report_json(it->second));
      } else send_response(s,404,R"({"message":"Endpoint not found"})");
    } else send_response(s,400,R"({"message":"Unsupported HTTP method"})");
  } catch(const std::exception& e) {
    send_response(s,500,"{\"message\":"+js(e.what())+"}");
  }
  closesocket(s);
}

} // namespace

int main(int argc, char** argv) {
  int port=8080;
  for(int i=1;i+1<argc;++i) if(std::string(argv[i])=="--port") port=std::stoi(argv[++i]);
  WSADATA data{};
  if(WSAStartup(MAKEWORD(2,2),&data)!=0) return 1;
  SOCKET server=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
  if(server==INVALID_SOCKET){WSACleanup();return 1;}
  sockaddr_in addr{}; addr.sin_family=AF_INET; addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK); addr.sin_port=htons(static_cast<u_short>(port));
  if(bind(server,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))==SOCKET_ERROR || listen(server,16)==SOCKET_ERROR){closesocket(server);WSACleanup();return 1;}
  std::cout<<"Indigenous Phase 6 API listening on http://127.0.0.1:"<<port<<"\n";
  for(;;){SOCKET client=accept(server,nullptr,nullptr);if(client!=INVALID_SOCKET)std::thread(handle,client).detach();}
  closesocket(server); WSACleanup(); return 0;
}
