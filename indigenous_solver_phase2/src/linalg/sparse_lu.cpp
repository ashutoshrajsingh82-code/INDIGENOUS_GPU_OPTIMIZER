#include "solver/linalg/sparse_lu.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace solver {
std::size_t SparseLU::l_nonzeros() const {
  std::size_t count=0;
  for(const auto& row:l_) count+=row.size();
  return count;
}
std::size_t SparseLU::u_nonzeros() const {
  std::size_t count=0;
  for(const auto& row:u_) count+=row.size();
  return count;
}
bool SparseLU::factorize(const std::vector<std::vector<Real>>& a, Real tol) {
  n_=static_cast<Index>(a.size());
  tol_=std::max<Real>(tol, 0);
  // Keep pivot acceptance strict, but use a much smaller drop tolerance so
  // small fill-in is not discarded before it can stabilize ill-conditioned
  // Netlib bases.
  const Real drop_tol=std::max<Real>(100*std::numeric_limits<Real>::epsilon(), tol_*1e-3);
  min_pivot_=std::numeric_limits<Real>::infinity();
  l_.clear();
  u_.clear();
  perm_.clear();
  etas_.clear();

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
      if(std::abs(a[i][j])>drop_tol) u_[i][j]=a[i][j];
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
      if(std::abs(multiplier)>drop_tol) l_[i][k]=multiplier;

      // U[k] is sparse; propagate only its entries after the pivot.
      for(const auto& [j, value] : u_[k]) {
        if(j<=k) continue;
        const Real updated=u_[i].count(j)?u_[i][j]-multiplier*value
                                        :-multiplier*value;
        if(std::abs(updated)<=drop_tol)
          u_[i].erase(j);
        else
          u_[i][j]=updated;
      }
      for(auto it=u_[i].begin(); it!=u_[i].end(); ) {
        if(std::abs(it->second)<=drop_tol) it=u_[i].erase(it);
        else ++it;
      }
    }
  }

  return std::isfinite(min_pivot_);
}

bool SparseLU::update(const std::vector<Real>& direction,
                     Index leaving_row, Real pivot_tolerance) {
  if(n_==0 || static_cast<Index>(direction.size())!=n_ || leaving_row>=n_)
    return false;

  const Real pivot=direction[leaving_row];
  if(!std::isfinite(pivot) ||
     std::abs(pivot)<=pivot_tolerance*std::max<Real>(1.0,
                                                     std::abs(pivot)))
    return false;

  EtaUpdate eta;
  eta.pivot_row=leaving_row;
  eta.pivot=pivot;
  eta.direction=direction;
  etas_.push_back(std::move(eta));
  return true;
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

  // B_current = B_base * E_1 * ... * E_k. For an eta matrix E whose
  // replacement column is d, E*x=y gives x[r]=y[r]/d[r] and
  // x[i]=y[i]-d[i]*x[r]. Apply E_1^{-1}, then E_2^{-1}, ... in order.
  for(const auto& eta:etas_) {
    const Real pivot_component=x[eta.pivot_row]/eta.pivot;
    for(Index i=0;i<n_;++i) if(i!=eta.pivot_row)
      x[i]-=eta.direction[i]*pivot_component;
    x[eta.pivot_row]=pivot_component;
  }
  return true;
}

bool SparseLU::solve_transpose(const std::vector<Real>& b,
                               std::vector<Real>& x) const {
  if(n_==0 || static_cast<Index>(b.size())!=n_) return false;

  // A^T*x=b and P*A=L*U imply:
  // U^T*y=b, L^T*z=y, P*x=z.
  // B_current^T = E_k^T ... E_1^T B_base^T. Solve the eta system first,
  // in reverse update order, then solve the base transpose factors.
  std::vector<Real> transformed=b;
  for(auto it=etas_.rbegin();it!=etas_.rend();++it) {
    const auto& eta=*it;
    Real sum=0;
    for(Index i=0;i<n_;++i) if(i!=eta.pivot_row)
      sum+=eta.direction[i]*transformed[i];
    transformed[eta.pivot_row]=(transformed[eta.pivot_row]-sum)/eta.pivot;
  }

  // U^T*y=transformed. Scatter each solved component through the
  // existing sparse row entries instead of scanning all preceding columns.
  // This changes the transpose solve from dense O(n^2) hash lookups to
  // work proportional to the stored U nonzeros.
  std::vector<Real> y=transformed;
  for(Index i=0;i<n_;++i) {
    auto diag=u_[i].find(i);
    if(diag==u_[i].end() || std::abs(diag->second)<=tol_) return false;
    y[i]/=diag->second;
    for(const auto& [j,value] : u_[i]) {
      if(j>i) y[j]-=value*y[i];
    }
  }

  // L has an implicit unit diagonal. Solve L^T*z=y by processing rows
  // backwards and scattering each solved value into earlier columns.
  std::vector<Real> z=y;
  for(Index i=n_; i-->0; ) {
    for(const auto& [j,value] : l_[i]) {
      if(j<i) z[j]-=value*z[i];
    }
  }

  x.assign(n_,0);
  for(Index i=0;i<n_;++i) x[perm_[i]]=z[i];
  return true;
}

} // namespace solver
