#include "solver/model/solution_validator.hpp"
#include <cmath>
#include <algorithm>
namespace solver {
ValidationReport validate_solution(const LinearModel&m,const std::vector<Real>&x,Real reported){
  ValidationReport r;
  constexpr Real feasibility_tolerance=1e-7;
  auto allowed=[&](Real scale){return feasibility_tolerance*std::max<Real>(1.0,std::abs(scale));};
  if(x.size()!=m.variables.size()){r.message="primal vector dimension mismatch";return r;}
  for(size_t j=0;j<x.size();++j){
    if(std::isnan(x[j])||std::isinf(x[j])){r.message="non-finite primal value";return r;}
    if(std::isfinite(m.variables[j].lower_bound)&&x[j]<m.variables[j].lower_bound)
      r.max_primal_violation=std::max(r.max_primal_violation,m.variables[j].lower_bound-x[j]);
    if(std::isfinite(m.variables[j].upper_bound)&&x[j]>m.variables[j].upper_bound)
      r.max_primal_violation=std::max(r.max_primal_violation,x[j]-m.variables[j].upper_bound);
  }
  std::vector<Real> ax;
  m.A.multiply(x,ax);
  for(size_t i=0;i<ax.size();++i){
    if(std::isfinite(m.constraints[i].lower_bound)&&ax[i]<m.constraints[i].lower_bound)
      r.max_primal_violation=std::max(r.max_primal_violation,m.constraints[i].lower_bound-ax[i]);
    if(std::isfinite(m.constraints[i].upper_bound)&&ax[i]>m.constraints[i].upper_bound)
      r.max_primal_violation=std::max(r.max_primal_violation,ax[i]-m.constraints[i].upper_bound);
  }
  r.recomputed_objective=m.objective_value(x);
  r.objective_difference=std::abs(r.recomputed_objective-reported);
  Real feasibility_scale=1.0;
  for(const auto&v:m.variables){
    if(std::isfinite(v.lower_bound))feasibility_scale=std::max(feasibility_scale,std::abs(v.lower_bound));
    if(std::isfinite(v.upper_bound))feasibility_scale=std::max(feasibility_scale,std::abs(v.upper_bound));
  }
  for(const auto&c:m.constraints){
    if(std::isfinite(c.lower_bound))feasibility_scale=std::max(feasibility_scale,std::abs(c.lower_bound));
    if(std::isfinite(c.upper_bound))feasibility_scale=std::max(feasibility_scale,std::abs(c.upper_bound));
  }
  r.valid=r.max_primal_violation<=allowed(feasibility_scale);
  r.message=r.valid?"solution satisfies primal feasibility":"primal feasibility violation";
  return r;
}
}