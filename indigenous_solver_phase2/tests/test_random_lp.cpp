#include <cmath>
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>

#include "solver/model/solution_validator.hpp"
#include "solver/simplex/revised_simplex.hpp"

using namespace solver;

static int fails=0;

static void check(bool ok, const char* message) {
  if(!ok) {
    std::cerr << "FAIL: " << message << "\n";
    ++fails;
  }
}

int main() {
  // Deterministic randomized regression. Every generated LP has a known
  // optimum: all variables have nonnegative objective coefficients after the
  // minimization sign convention, and their explicit upper bounds are feasible.
  // Additional nonnegative rows are constructed so that the upper-bound point
  // satisfies them exactly. Therefore the known optimum is x = upper_bounds.
  std::mt19937 rng(26119);
  std::uniform_real_distribution<Real> upper_dist(0.5, 8.0);
  std::uniform_real_distribution<Real> coeff_dist(0.1, 4.0);
  std::uniform_real_distribution<Real> obj_dist(0.5, 5.0);
  std::uniform_int_distribution<int> n_dist(2, 6);
  std::uniform_int_distribution<int> m_dist(2, 8);

  for(int case_id=0; case_id<50; ++case_id) {
    const Index n=static_cast<Index>(n_dist(rng));
    const Index m=static_cast<Index>(m_dist(rng));

    LinearModel model;
    model.name="random-regression-"+std::to_string(case_id);
    model.minimize=true;
    model.variables.resize(n);

    std::vector<Real> upper(n,0);
    Real expected=0;
    for(Index j=0;j<n;++j) {
      upper[j]=upper_dist(rng);
      const Real objective=-obj_dist(rng);
      model.variables[j]={"x"+std::to_string(j),0,upper[j],objective,false};
      expected+=objective*upper[j];
    }

    std::vector<Index> row_indices;
    std::vector<Real> values;
    std::vector<Index> col_ptr(n+1,0);
    std::vector<Real> rhs(m,0);

    // Build dense random rows, stored as CSC.
    std::vector<std::vector<Real>> dense(m,std::vector<Real>(n,0));
    for(Index i=0;i<m;++i) {
      for(Index j=0;j<n;++j) {
        dense[i][j]=coeff_dist(rng);
        rhs[i]+=dense[i][j]*upper[j];
      }
    }

    for(Index j=0;j<n;++j) {
      col_ptr[j]=static_cast<Index>(values.size());
      for(Index i=0;i<m;++i) {
        row_indices.push_back(i);
        values.push_back(dense[i][j]);
      }
    }
    col_ptr[n]=static_cast<Index>(values.size());

    model.constraints.resize(m);
    for(Index i=0;i<m;++i)
      model.constraints[i]={"r"+std::to_string(i),-kInfinity,rhs[i]};
    model.A=CscMatrix(m,n,values,row_indices,col_ptr);

    RevisedSimplexOptions options;
    options.max_iterations=10000;
    auto result=RevisedSimplexSolver{options}.solve(model);

    check(result.status==SolveStatus::Optimal,"random LP optimal status");
    check(std::abs(result.objective_value-expected)<1e-6,
          "random LP known objective");

    if(static_cast<Index>(result.primal.size())==n) {
      for(Index j=0;j<n;++j)
        check(std::abs(result.primal[j]-upper[j])<1e-6,
              "random LP reaches known optimum");
    } else {
      check(false,"random LP primal size");
    }

    auto certificate=validate_solution(model,result.primal,result.objective_value);
    check(certificate.valid,"random LP certificate");
  }

  std::cout << (fails ? "RANDOM LP TESTS FAILED" : "RANDOM LP TESTS PASSED") << "\n";
  return fails ? 1 : 0;
}
