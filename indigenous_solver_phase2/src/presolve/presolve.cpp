#include "solver/presolve/presolve.hpp"
#include <cmath>
#include <limits>
namespace solver {
PresolveStats presolve(LinearModel& m){
  PresolveStats s;
  const Index nr=m.A.rows(), nc=m.A.cols();
  std::vector<int> row_nz(nr,0), col_nz(nc,0);
  for(Index j=0;j<nc;++j) for(Index p=m.A.column_pointers()[j];p<m.A.column_pointers()[j+1];++p){++col_nz[j];++row_nz[m.A.row_indices()[p]];}
  // Safe reductions that preserve the original model shape. Structural removal is
  // intentionally postponed because postsolve mappings are introduced in the next gate.
  for(Index j=0;j<nc;++j) if(m.variables[j].lower_bound==m.variables[j].upper_bound && std::isfinite(m.variables[j].lower_bound)) ++s.fixed_variables;
  for(Index i=0;i<nr;++i) if(row_nz[i]==0){
    const auto& c=m.constraints[i];
    if(c.lower_bound<=0 && 0<=c.upper_bound) ++s.empty_rows;
  }
  for(Index j=0;j<nc;++j) if(col_nz[j]==0) ++s.empty_columns;
  return s;
}
}
