#include "solver/linalg/sparse_lu.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace solver {
bool SparseLU::factorize(const std::vector<std::vector<Real>>& a, Real tol) {
  n_=static_cast<Index>(a.size()); tol_=tol; min_pivot_=std::numeric_limits<Real>::infinity();
  if(n_==0) return false;
  lu_=a; piv_.resize(n_);
  for(Index i=0;i<n_;++i) {
    if(static_cast<Index>(lu_[i].size())!=n_) return false;
    piv_[i]=i;
  }
  for(Index k=0;k<n_;++k) {
    Index p=k; Real scale=0;
    for(Index i=k;i<n_;++i) scale=std::max(scale,std::abs(lu_[i][k]));
    if(scale<=tol_) return false;
    Real best=std::abs(lu_[k][k]);
    for(Index i=k+1;i<n_;++i) if(std::abs(lu_[i][k])>best){best=std::abs(lu_[i][k]);p=i;}
    if(best<=tol_*std::max<Real>(1,scale)) return false;
    if(p!=k){std::swap(lu_[p],lu_[k]);std::swap(piv_[p],piv_[k]);}
    min_pivot_=std::min(min_pivot_,std::abs(lu_[k][k]));
    for(Index i=k+1;i<n_;++i) {
      if(std::abs(lu_[i][k])<=tol_) { lu_[i][k]=0; continue; }
      lu_[i][k]/=lu_[k][k];
      for(Index j=k+1;j<n_;++j) {
        lu_[i][j]-=lu_[i][k]*lu_[k][j];
        if(std::abs(lu_[i][j])<tol_) lu_[i][j]=0;
      }
    }
  }
  return true;
}
bool SparseLU::solve(const std::vector<Real>& b,std::vector<Real>& x) const {
  if(static_cast<Index>(b.size())!=n_||n_==0)return false;
  x.resize(n_);
  for(Index i=0;i<n_;++i)x[i]=b[piv_[i]];
  for(Index i=0;i<n_;++i)for(Index j=0;j<i;++j)x[i]-=lu_[i][j]*x[j];
  for(Index ii=n_-1;ii>=0;--ii){if(std::abs(lu_[ii][ii])<=tol_)return false;for(Index j=ii+1;j<n_;++j)x[ii]-=lu_[ii][j]*x[j];x[ii]/=lu_[ii][ii];}
  return true;
}
bool SparseLU::solve_transpose(const std::vector<Real>& b,std::vector<Real>& x) const {
  if(static_cast<Index>(b.size())!=n_||n_==0)return false;
  // U^T y=b, L^T z=y, then apply the row permutation.
  std::vector<Real> y=b;
  for(Index i=0;i<n_;++i){for(Index j=0;j<i;++j)y[i]-=lu_[j][i]*y[j];if(std::abs(lu_[i][i])<=tol_)return false;y[i]/=lu_[i][i];}
  for(Index ii=n_-1;ii>=0;--ii){for(Index j=ii+1;j<n_;++j)y[ii]-=lu_[j][ii]*y[j];}
  x.assign(n_,0);
  for(Index i=0;i<n_;++i)x[piv_[i]]=y[i];
  return true;
}
}
