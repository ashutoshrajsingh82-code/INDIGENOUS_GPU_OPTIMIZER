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

  SparseLU pivoted;
  check(pivoted.factorize({{2,1},{4,3}}),"pivoted LU factor");
  std::vector<Real> pb{5,6}, px, pxt;
  check(pivoted.solve(pb,px),"pivoted LU solve");
  check(std::abs(px[0]-4.5)<1e-9&&std::abs(px[1]+4.0)<1e-9,"pivoted LU answer");
  check(pivoted.solve_transpose(pb,pxt),"pivoted LU transpose solve");
  check(std::abs(pxt[0]+4.5)<1e-9&&std::abs(pxt[1]-3.5)<1e-9,"pivoted LU transpose answer");

  LinearModel m; m.name="phase2-test"; m.minimize=true;
  m.variables={{"x",0,4,-3,false},{"y",0,3,-2,false}};
  m.constraints={{"capacity",-kInfinity,2}};
  m.A=CscMatrix(1,2,{1,1},{0,0},{0,1,2});
  auto r=RevisedSimplexSolver{}.solve(m);
  check(r.status==SolveStatus::Optimal,"revised simplex optimal");
  check(std::abs(r.objective_value+6)<1e-7,"revised simplex objective");
  auto v=validate_solution(m,r.primal,r.objective_value);
  check(v.valid,"certificate");

  RevisedSimplexOptions no_devex; no_devex.use_devex=false;
  auto plain=RevisedSimplexSolver{no_devex}.solve(m);
  check(plain.status==SolveStatus::Optimal,"unit-weight pricing optimal");
  check(std::abs(plain.objective_value-r.objective_value)<1e-7,"Devex objective agreement");

  RevisedSimplexOptions devex; devex.use_devex=true;
  auto weighted=RevisedSimplexSolver{devex}.solve(m);
  check(weighted.status==SolveStatus::Optimal,"Devex pricing optimal");
  check(std::abs(weighted.objective_value-r.objective_value)<1e-7,"Devex objective agreement");

  LinearModel deg;
  deg.name="degenerate-test"; deg.minimize=true;
  deg.variables={{"x",0,kInfinity,-1,false},{"y",0,kInfinity,-1,false}};
  deg.constraints={{"x_zero",-kInfinity,0},{"x_y_cap",-kInfinity,1}};
  deg.A=CscMatrix(2,2,{1,1,1},{0,1,1},{0,2,3});
  auto dr=RevisedSimplexSolver{}.solve(deg);
  check(dr.status==SolveStatus::Optimal,"degenerate Harris optimal");
  check(std::abs(dr.objective_value+1)<1e-7,"degenerate Harris objective");
  check(std::abs(dr.primal[0])<1e-7&&std::abs(dr.primal[1]-1)<1e-7,"degenerate Harris primal");
  auto dv=validate_solution(deg,dr.primal,dr.objective_value);
  check(dv.valid,"degenerate Harris certificate");

  // Equality: x + y = 2, with the same bounds/objective as the canonical test.
  LinearModel eq=m; eq.constraints={{"eq",2,2}};
  auto er=RevisedSimplexSolver{}.solve(eq);
  check(er.status==SolveStatus::Optimal,"equality Phase I/II optimal");
  check(std::abs(er.objective_value+6)<1e-7,"equality objective");
  check(std::abs(er.primal[0]-2)<1e-7&&std::abs(er.primal[1])<1e-7,"equality primal");
  auto ev=validate_solution(eq,er.primal,er.objective_value);
  check(ev.valid,"equality certificate");

  // >=: x + y >= 2, while upper bounds keep the maximization bounded.
  LinearModel ge;
  ge.name="greater-than-test"; ge.minimize=true;
  ge.variables={{"x",0,2,-1,false},{"y",0,3,-1,false}};
  ge.constraints={{"demand",2,kInfinity}};
  ge.A=CscMatrix(1,2,{1,1},{0,0},{0,1,2});
  auto gr=RevisedSimplexSolver{}.solve(ge);
  check(gr.status==SolveStatus::Optimal,">= Phase I/II optimal");
  check(std::abs(gr.objective_value+5)<1e-7,">= objective");
  check(std::abs(gr.primal[0]-2)<1e-7&&std::abs(gr.primal[1]-3)<1e-7,">= primal");
  auto gv=validate_solution(ge,gr.primal,gr.objective_value);
  check(gv.valid,">= certificate");

  // Infeasible equality must be detected by Phase I rather than accepted as optimal.
  LinearModel infeasible=m;
  infeasible.constraints={{"impossible",3,2}};
  auto ir=RevisedSimplexSolver{}.solve(infeasible);
  check(ir.status==SolveStatus::Infeasible,"Phase I infeasibility");

  std::cout<<(fails?"PHASE 2 TESTS FAILED":"PHASE 2 TESTS PASSED")<<"\n";
  return fails?1:0;
}
