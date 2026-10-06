#include "gpu/basis_solver.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace {
using Workspace = indigenous::gpu::BasisSolveWorkspace;
using Row = Workspace::SparseRow;
using Rows = Workspace::SparseRows;

bool near(double a, double b, double tol = 1e-11) {
  return std::abs(a - b) <= tol * std::max({1.0, std::abs(a), std::abs(b)});
}

bool check_vector(const std::vector<double>& actual,
                  const std::vector<double>& expected) {
  if (actual.size() != expected.size()) return false;
  for (std::size_t i = 0; i < actual.size(); ++i)
    if (!near(actual[i], expected[i])) return false;
  return true;
}
} // namespace

int main() {
  // P*A=L*U with a non-identity permutation:
  //
  // L = [1 0 0; 2 1 0; -1 3 1]
  // U = [2 1 0; 0 3 4; 0 0 5]
  // P = [0 2 1] in row-index form.
  Rows L = {
      Row{},
      Row{{0, 2.0}},
      Row{{0, -1.0}, {1, 3.0}}};
  Rows U = {
      Row{{1, 1.0}},
      Row{{2, 4.0}},
      Row{}};
  std::vector<double> diag{2.0, 3.0, 5.0};
  std::vector<std::size_t> permutation{0, 2, 1};

  Workspace workspace;
  if (!workspace.initialize(L, U, diag, permutation)) {
    std::cerr << "Basis workspace initialization: FAIL
";
    return 1;
  }

  // Expected FTRAN result is obtained from the equivalent explicit basis.
  // A = P^T * L * U.
  const std::vector<std::vector<double>> A = {
      {2.0, 1.0, 0.0},
      {-2.0, 8.0, 17.0},
      {4.0, 5.0, 4.0}};
  const std::vector<double> b{3.0, -10.0, 7.0};

  std::vector<double> x;
  if (!workspace.ftran(b, x)) {
    std::cerr << "FTRAN: FAIL
";
    return 1;
  }

  // A*x=b gives x=[2, -1, 1].
  const std::vector<double> expected_x{2.0, -1.0, 1.0};
  if (!check_vector(x, expected_x)) {
    std::cerr << "FTRAN value check: FAIL
";
    return 1;
  }

  // Verify BTRAN independently using A^T*y=c.
  const std::vector<double> c{10.0, 32.0, 46.0};
  std::vector<double> y;
  if (!workspace.btran(c, y)) {
    std::cerr << "BTRAN: FAIL
";
    return 1;
  }

  // A^T*[1,2,3] = [4,21,60].
  const std::vector<double> expected_y{1.0, 2.0, 3.0};
  if (!check_vector(y, expected_y)) {
    std::cerr << "BTRAN value check: FAIL
";
    return 1;
  }

  if (workspace.last_ftran_ms() < 0.0 || workspace.last_btran_ms() < 0.0) {
    std::cerr << "Timing instrumentation: FAIL
";
    return 1;
  }

  std::cout << "Phase 4 basis solver CPU test: PASS
";
  return 0;
}
