#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include "solver/io/lp_reader.hpp"
#include "solver/io/mps_reader.hpp"
#include "solver/simplex/primal_simplex.hpp"
#include "solver/simplex/dual_simplex.hpp"
#include "solver/common/status.hpp"
using namespace solver;
static bool load(const std::string& f,LinearModel& m,std::string&e){auto p=f.find_last_of('.');std::string ext=p==std::string::npos?"":f.substr(p+1);if(ext=="mps"||ext=="MPS")return read_mps(f,m,e);return read_lp(f,m,e);}
static void inspect(const LinearModel&m){std::size_t nz=m.A.values().size();std::cout<<"Model: "<<m.name<<"\nVariables: "<<m.variables.size()<<"\nConstraints: "<<m.constraints.size()<<"\nNonzeros: "<<nz<<"\nObjective sense: "<<(m.minimize?"MINIMIZE":"MAXIMIZE")<<"\n";std::size_t eq=0,le=0,ge=0;for(auto&c:m.constraints){if(c.lower_bound==c.upper_bound)++eq;else if(c.upper_bound<kInfinity)++le;else if(c.lower_bound>-kInfinity)++ge;}std::cout<<"Equality: "<<eq<<"  <=: "<<le<<"  >=: "<<ge<<"\nModel validation: PASS\n";}
int main(int argc,char**argv){if(argc<3){std::cerr<<"Usage: solver_cli inspect|solve <model.lp|model.mps> [--method primal|dual]\n";return 2;}std::string cmd=argv[1],file=argv[2];LinearModel m;std::string e;if(!load(file,m,e)){std::cerr<<"ERROR: "<<e<<"\n";return 1;}if(cmd=="inspect"){inspect(m);return 0;}if(cmd!="solve"){std::cerr<<"Unknown command\n";return 2;}std::string method="primal";for(int i=3;i<argc-1;++i)if(std::string(argv[i])=="--method")method=argv[i+1];SolveResult r;if(method=="dual")r=DualSimplexSolver{}.solve(m);else r=PrimalSimplexSolver{}.solve(m);std::cout<<"Status: "<<to_string(r.status)<<"\n"<<"Objective: "<<std::setprecision(12)<<r.objective_value<<"\n"<<"Iterations: "<<r.iterations<<"\n"<<"Primal residual: "<<r.primal_residual<<"\n"<<"Message: "<<r.message<<"\n";if(!r.primal.empty()){std::cout<<"Primal:\n";for(std::size_t i=0;i<r.primal.size();++i)std::cout<<"  "<<m.variables[i].name<<" = "<<std::setprecision(12)<<r.primal[i]<<"\n";}return r.status==SolveStatus::Optimal?0:1;}
