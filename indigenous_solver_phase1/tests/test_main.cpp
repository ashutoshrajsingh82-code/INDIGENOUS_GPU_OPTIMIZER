#include <iostream>
#include <cmath>
#include <fstream>
#include <filesystem>
#include "solver/sparse/csc_matrix.hpp"
#include "solver/sparse/csr_matrix.hpp"
#include "solver/linalg/lu.hpp"
#include "solver/io/lp_reader.hpp"
#include "solver/io/mps_reader.hpp"
#include "solver/simplex/primal_simplex.hpp"
#include "solver/model/solution_validator.hpp"
using namespace solver;
static int fails=0;static void check(bool x,const char*msg){if(!x){std::cerr<<"FAIL: "<<msg<<"\n";++fails;}}
int main(){
 CscMatrix c(2,2,{1,2,3},{0,1,1},{0,1,3});std::vector<Real> x{2,4},y;c.multiply(x,y);check(y[0]==2&&y[1]==20,"CSC multiply");auto r=c.to_csr();r.multiply(x,y);check(y[0]==2&&y[1]==20,"CSR multiply");
 LUFactorization lu;check(lu.factorize({{2,1},{1,3}}),"LU factor");std::vector<Real>b{5,6},s;check(lu.solve(b,s),"LU solve");check(std::abs(s[0]-1.8)<1e-9&&std::abs(s[1]-1.4)<1e-9,"LU answer");check(!lu.factorize({{1,2},{2,4}}),"LU singular");
 LinearModel m;std::string e;check(read_lp("../examples/lp/simple.lp",m,e),"LP parser");check(m.variables.size()==2&&m.constraints.size()==3,"LP dimensions");LinearModel mm;check(read_mps("../examples/lp/simple.mps",mm,e),"MPS parser");check(mm.variables.size()==2&&mm.constraints.size()==3,"MPS dimensions");
 auto res=PrimalSimplexSolver{}.solve(m);check(res.status==SolveStatus::Optimal,"simplex optimal");check(std::abs(res.objective_value+10)<1e-7,"simplex objective");auto vr=validate_solution(m,res.primal,res.objective_value);check(vr.valid&&vr.max_primal_violation<1e-8,"solution validator");
 std::cout<<(fails?"FAILED":"ALL TESTS PASSED")<<"\n";return fails?1:0;}
