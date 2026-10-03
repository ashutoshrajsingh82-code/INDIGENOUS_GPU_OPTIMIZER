#include "solver/scaling/scaler.hpp"
#include <algorithm>
#include <cmath>
namespace solver {
ScalingResult scale_model(LinearModel& m){
  ScalingResult out; const Index nr=m.A.rows(), nc=m.A.cols();
  out.row_scale.assign(nr,1); out.col_scale.assign(nc,1);
  std::vector<Real> rmax(nr,0),cmax(nc,0);
  for(Index j=0;j<nc;++j) for(Index p=m.A.column_pointers()[j];p<m.A.column_pointers()[j+1];++p){
    const Index i=m.A.row_indices()[p]; const Real a=std::abs(m.A.values()[p]);
    rmax[i]=std::max(rmax[i],a); cmax[j]=std::max(cmax[j],a); out.max_abs=std::max(out.max_abs,a);
  }
  // Powers of two keep scaling reproducible and avoid introducing avoidable roundoff.
  auto pow2=[](Real x){ if(x<=0)return Real(1); int e=0; std::frexp(x,&e); return std::ldexp(1.0,-e); };
  for(Index i=0;i<nr;++i) if(rmax[i]>0) out.row_scale[i]=pow2(rmax[i]);
  for(Index j=0;j<nc;++j) if(cmax[j]>0) out.col_scale[j]=pow2(cmax[j]);
  return out;
}
}
