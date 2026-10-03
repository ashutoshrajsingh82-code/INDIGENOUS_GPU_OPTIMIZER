#include <cmath>
#include <iostream>
#include "solver/linalg/sparse_lu.hpp"
#include "solver/simplex/revised_simplex.hpp"
#include "solver/model/solution_validator.hpp"
using namespace solver;
static int fails=0;
static void check(bool x,const char* m){if(!x){std::cerr<<"FAIL: "<<m<<"\n";++fails;}}
int main(){
  SparseLU lu;
  check(lu.factorize({{2,1},{1,3}}),"LU factor");
  std::vector<Real>b{5,6},x;
  check(lu.solve(b,x),"LU solve");
  check(std::abs(x[0]-1.8)<1e-9&&std::abs(x[1]-1.4)<1e-9,"LU answer");

  LinearModel m; m.name="phase2-test"; m.minimize=true;
  m.variables={{"x",0,4,-3,false},{"y",0,3,-2,false}};
  m.constraints={{"capacity",-kInfinity,2}};
  m.A=CscMatrix(1,2,{1,1},{0,0},{0,1,2});
  auto r=RevisedSimplexSolver{}.solve(m);
  check(r.status==SolveStatus::Optimal,"revised simplex optimal");
  check(std::abs(r.objective_value+10)<1e-7,"revised simplex objective");
  auto v=validate_solution(m,r.primal,r.objective_value);
  check(v.valid,"certificate");

  LinearModel eq=m; eq.constraints={{"eq",2,2}};
  auto er=RevisedSimplexSolver{}.solve(eq);
  check(er.status==SolveStatus::UnsupportedModel,"equality gate");

  std::cout<<(fails?"PHASE 2 TESTS FAILED":"PHASE 2 TESTS PASSED")<<"\n";
  return fails?1:0;
}