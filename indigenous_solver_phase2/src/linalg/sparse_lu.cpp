#include "solver/linalg/sparse_lu.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace solver {
bool SparseLU::factorize(const std::vector<std::vector<Real>>& a, Real tol) {
  n_=static_cast<Index>(a.size());
  tol_=std::max<Real>(tol, 0);
  min_pivot_=std::numeric_limits<Real>::infinity();
  l_.clear();
  u_.clear();
  perm_.clear();

  if (n_==0) return false;
  u_.resize(n_);
  l_.resize(n_);
  perm_.resize(n_);

  for(Index i=0;i<n_;++i) {
    if(static_cast<Index>(a[i].size())!=n_) {
      n_=0;
      return false;
    }
    perm_[i]=i;
    for(Index j=0;j<n_;++j) {
      if(std::abs(a[i][j])>tol_) u_[i][j]=a[i][j];
    }
  }

  for(Index k=0;k<n_;++k) {
    Index pivot=k;
    Real column_scale=0;
    for(Index i=k;i<n_;++i) {
      auto it=u_[i].find(k);
      if(it!=u_[i].end()) {
        column_scale=std::max(column_scale,std::abs(it->second));
        if(std::abs(it->second)>std::abs(u_[pivot].count(k)?u_[pivot].at(k):0))
          pivot=i;
      }
    }

    if(column_scale<=tol_) {
      n_=0;
      return false;
    }

    if(pivot!=k) {
      std::swap(u_[pivot],u_[k]);
      std::swap(l_[pivot],l_[k]);
      std::swap(perm_[pivot],perm_[k]);
    }

    auto diag_it=u_[k].find(k);
    if(diag_it==u_[k].end() ||
       std::abs(diag_it->second)<=tol_*std::max<Real>(1,column_scale)) {
      n_=0;
      return false;
    }

    const Real pivot_value=diag_it->second;
    min_pivot_=std::min(min_pivot_,std::abs(pivot_value));

    // Only rows with a nonzero in the pivot column require elimination.
    for(Index i=k+1;i<n_;++i) {
      auto col_it=u_[i].find(k);
      if(col_it==u_[i].end()) continue;

      const Real multiplier=col_it->second/pivot_value;
      u_[i].erase(col_it);
      if(std::abs(multiplier)>tol_) l_[i][k]=multiplier;

      // U[k] is sparse; propagate only its entries after the pivot.
      for(const auto& [j, value] : u_[k]) {
        if(j<=k) continue;
        const Real updated=u_[i].count(j)?u_[i][j]-multiplier*value
                                        :-multiplier*value;
        if(std::abs(updated)<=tol_)
          u_[i].erase(j);
        else
          u_[i][j]=updated;
      }
      for(auto it=u_[i].begin(); it!=u_[i].end(); ) {
        if(std::abs(it->second)<=tol_) it=u_[i].erase(it);
        else ++it;
      }
    }
  }

  return std::isfinite(min_pivot_);
}

bool SparseLU::solve(const std::vector<Real>& b,
                     std::vector<Real>& x) const {
  if(n_==0 || static_cast<Index>(b.size())!=n_) return false;

  // P*b = L*y.
  std::vector<Real> y(n_,0);
  for(Index i=0;i<n_;++i) {
    y[i]=b[perm_[i]];
    for(const auto& [j,value] : l_[i])
      if(j<i) y[i]-=value*y[j];
  }

  // U*x=y.
  x.assign(n_,0);
  for(Index ii=n_; ii-->0; ) {
    Real rhs=y[ii];
    for(const auto& [j,value] : u_[ii])
      if(j>ii) rhs-=value*x[j];

    auto diag=u_[ii].find(ii);
    if(diag==u_[ii].end() || std::abs(diag->second)<=tol_) return false;
    x[ii]=rhs/diag->second;
  }
  return true;
}

bool SparseLU::solve_transpose(const std::vector<Real>& b,
                               std::vector<Real>& x) const {
  if(n_==0 || static_cast<Index>(b.size())!=n_) return false;

  // A^T*x=b and P*A=L*U imply:
  // U^T*y=b, L^T*z=y, P*x=z.
  std::vector<Real> y=b;
  for(Index i=0;i<n_;++i) {
    auto diag=u_[i].find(i);
    if(diag==u_[i].end() || std::abs(diag->second)<=tol_) return false;
    for(Index j=0;j<i;++j) {
      auto it=u_[j].find(i);
      if(it!=u_[j].end()) y[i]-=it->second*y[j];
    }
    y[i]/=diag->second;
  }

  std::vector<Real> z=y;
  for(Index i=n_; i-->0; ) {
    for(Index j=i+1;j<n_;++j) {
      auto it=l_[j].find(i);
      if(it!=l_[j].end()) z[i]-=it->second*z[j];
    }
  }

  x.assign(n_,0);
  for(Index i=0;i<n_;++i) x[perm_[i]]=z[i];
  return true;
}

} // namespace solver
