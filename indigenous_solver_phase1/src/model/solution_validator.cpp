#include "solver/model/solution_validator.hpp"
#include <cmath>
#include <algorithm>
namespace solver {
ValidationReport validate_solution(const LinearModel&m,const std::vector<Real>&x,Real reported){
  ValidationReport r;
  constexpr Real feasibility_tolerance=2e-7;
  bool feasible=true;
  auto check_violation=[&](Real violation,Real scale){
    if(violation>0){
      r.max_primal_violation=std::max(r.max_primal_violation,violation);
      if(violation>feasibility_tolerance*std::max<Real>(1.0,std::abs(scale)))feasible=false;
    }
  };
  if(x.size()!=m.variables.size()){r.message="primal vector dimension mismatch";return r;}
  for(size_t j=0;j<x.size();++j){
    if(std::isnan(x[j])||std::isinf(x[j])){r.message="non-finite primal value";return r;}
    if(std::isfinite(m.variables[j].lower_bound))
      check_violation(m.variables[j].lower_bound-x[j],std::max(std::abs(m.variables[j].lower_bound),std::abs(x[j])));
    if(std::isfinite(m.variables[j].upper_bound))
      check_violation(x[j]-m.variables[j].upper_bound,std::max(std::abs(m.variables[j].upper_bound),std::abs(x[j])));
  }
  std::vector<Real> ax;
  m.A.multiply(x,ax);
  for(size_t i=0;i<ax.size();++i){
    if(std::isfinite(m.constraints[i].lower_bound))
      check_violation(m.constraints[i].lower_bound-ax[i],std::max(std::abs(m.constraints[i].lower_bound),std::abs(ax[i])));
    if(std::isfinite(m.constraints[i].upper_bound))
      check_violation(ax[i]-m.constraints[i].upper_bound,std::max(std::abs(m.constraints[i].upper_bound),std::abs(ax[i])));
  }
  r.recomputed_objective=m.objective_value(x);
  r.objective_difference=std::abs(r.recomputed_objective-reported);
  r.valid=feasible;
  r.message=r.valid?"solution satisfies primal feasibility":"primal feasibility violation";
  return r;
}
}
