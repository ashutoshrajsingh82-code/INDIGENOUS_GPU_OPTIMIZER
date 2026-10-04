#include <cmath>
#include <iostream>
#include <fstream>
#include <cstdio>
#include "solver/linalg/sparse_lu.hpp"
#include "solver/simplex/revised_simplex.hpp"
#include "solver/model/solution_validator.hpp"
#include "solver/io/mps_reader.hpp"
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
  check(r.statistics.total_ms>=0,"simplex total timing");
  check(r.statistics.lu_factorizations>0,"simplex LU factorization count");
  check(r.statistics.btran_solves>0,"simplex BTRAN count");
  check(r.statistics.ftran_solves>0,"simplex FTRAN count");
  check(r.statistics.pivots>0,"simplex pivot count");
  check(r.statistics.lu_factorization_ms>=0,"simplex LU timing");
  check(r.statistics.pricing_ms>=0,"simplex pricing timing");

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

  // MPS parser regression: repeated entries for the same variable/row
  // must be aggregated rather than silently keeping only the first entry.
  {
    const char* path="phase2_mps_duplicate_test.mps";
    std::ofstream out(path);
    out << "NAME DUP\nROWS\n N OBJ\n L C1\nCOLUMNS\n X OBJ -1 C1 1\n X C1 2\nRHS\n RHS1 C1 3\nBOUNDS\nENDATA\n";
    out.close();
    LinearModel parsed; std::string parse_error;
    check(read_mps(path,parsed,parse_error),"MPS duplicate-entry parse");
    check(parsed.variables.size()==1 && parsed.constraints.size()==1,"MPS duplicate-entry dimensions");
    std::vector<Real> parsed_ax;
    parsed.A.multiply(std::vector<Real>{1},parsed_ax);
    check(parsed_ax.size()==1 && std::abs(parsed_ax[0]-3)<1e-12,"MPS duplicate-entry aggregation");
    check(std::abs(parsed.variables[0].objective+1)<1e-12,"MPS objective parse");
    std::remove(path);
  }

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

  // Infeasible but structurally valid model: x + y <= 1 and x + y >= 2.
  // The row bounds themselves are valid; infeasibility must be detected by Phase I.
  LinearModel infeasible=m;
  infeasible.constraints={{"upper", -kInfinity, 1},
                          {"lower", 2, kInfinity}};
  infeasible.A=CscMatrix(2,2,{1,1,1,1},{0,1,0,1},{0,2,4});
  auto ir=RevisedSimplexSolver{}.solve(infeasible);
  check(ir.status==SolveStatus::Infeasible,"Phase I infeasibility");

  std::cout<<(fails?"PHASE 2 TESTS FAILED":"PHASE 2 TESTS PASSED")<<"\n";
  return fails?1:0;
}
