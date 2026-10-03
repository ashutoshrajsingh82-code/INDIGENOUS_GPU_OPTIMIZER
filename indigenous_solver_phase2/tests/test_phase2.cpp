#include <cmath>
#include <iostream>
#include "solver/io/lp_reader.hpp"
#include "solver/simplex/revised_simplex.hpp"
#include "solver/model/solution_validator.hpp"
#include "solver/linalg/sparse_lu.hpp"
using namespace solver;
static int fails=0;static void check(bool x,const char*msg){if(!x){std::cerr<<"FAIL: "<<msg<<"\n";++fails;}}
int main(){
  SparseLU lu;check(lu.factorize({{2,1},{1,3}}),"SparseLU factor");
  std::vector<Real>b{5,6},x;check(lu.solve(b,x),"SparseLU solve");check(std::abs(x[0]-1.8)<1e-9&&std::abs(x[1]-1.4)<1e-9,"SparseLU answer");
  LinearModel m;std::string e;check(read_lp("../indigenous_solver_phase1/examples/lp/simple.lp",m,e),"read sample LP");
  auto r=RevisedSimplexSolver{}.solve(m);check(r.status==SolveStatus::Optimal,"Phase2 optimal");check(std::abs(r.objective_value+10)<1e-7,"Phase2 objective");
  auto v=validate_solution(m,r.primal,r.objective_value);check(v.valid,"Phase2 certificate");
  std::cout<<(fails?"PHASE 2 TESTS FAILED":"PHASE 2 TESTS PASSED")<<"\n";return fails?1:0;
}
