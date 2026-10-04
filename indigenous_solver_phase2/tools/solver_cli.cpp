#include <iomanip>
#include <iostream>
#include <string>
#include "solver/io/lp_reader.hpp"
#include "solver/io/mps_reader.hpp"
#include "solver/simplex/revised_simplex.hpp"
#include "solver/model/solution_validator.hpp"
using namespace solver;
static bool load(const std::string& f,LinearModel& m,std::string& e){
  auto p=f.find_last_of('.');auto ext=p==std::string::npos?"":f.substr(p+1);
  if(ext=="mps"||ext=="MPS")return read_mps(f,m,e); return read_lp(f,m,e);
}
int main(int argc,char**argv){
  if(argc<3){std::cerr<<"Usage: solver_phase2_cli solve <model.lp|model.mps> [--max-iters N]\n";return 2;}
  LinearModel m;std::string e;if(!load(argv[2],m,e)){std::cerr<<"ERROR: "<<e<<"\n";return 1;}
  RevisedSimplexOptions o;
  for(int i=3;i+1<argc;++i)if(std::string(argv[i])=="--max-iters")o.max_iterations=std::stoull(argv[++i]);
  auto r=RevisedSimplexSolver{o}.solve(m);
  std::cout<<"Status: "<<to_string(r.status)<<"\nObjective: "<<std::setprecision(12)<<r.objective_value
           <<"\nIterations: "<<r.iterations<<"\nPrimal residual: "<<r.primal_residual
           <<"\n"<<r.message<<"\n";
  const auto& s=r.statistics;
  std::cout<<"Timing total_ms: "<<std::setprecision(6)<<s.total_ms<<"\n"
           <<"Timing LU_factorization_ms: "<<s.lu_factorization_ms<<"\n"
           <<"Timing BTRAN_ms: "<<s.btran_ms<<"\n"
           <<"Timing FTRAN_ms: "<<s.ftran_ms<<"\n"
           <<"Timing pricing_ms: "<<s.pricing_ms<<"\n"
           <<"Timing pivot_ms: "<<s.pivot_ms<<"\n"
           <<"Timing ratio_test_ms: "<<s.ratio_test_ms<<"\n"
           <<"Timing basis_update_ms: "<<s.basis_update_ms<<"\n"
           <<"LU factorizations: "<<s.lu_factorizations<<"\n"
           <<"BTRAN solves: "<<s.btran_solves<<"\n"
           <<"FTRAN solves: "<<s.ftran_solves<<"\n"
           <<"Pivots: "<<s.pivots<<"\n"
           <<"Max LU nonzeros: "<<s.max_lu_nonzeros<<"\n";
  if(!r.primal.empty()){auto v=validate_solution(m,r.primal,r.objective_value);std::cout<<"Certificate: "<<(v.valid?"PASS":"FAIL")<<"\n"<<"Certificate max primal violation: "<<std::setprecision(12)<<v.max_primal_violation<<"\n"<<"Certificate objective difference: "<<std::setprecision(12)<<v.objective_difference<<"\n"<<v.message<<"\n";for(size_t i=0;i<r.primal.size();++i)std::cout<<"  "<<m.variables[i].name<<" = "<<std::setprecision(12)<<r.primal[i]<<"\n";}
  return r.status==SolveStatus::Optimal?0:1;
}
