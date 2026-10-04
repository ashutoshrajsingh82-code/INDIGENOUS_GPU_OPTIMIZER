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

  // Markowitz-style sparse pivoting with threshold partial pivoting.
  // Prefer pivots that minimize fill-in, but reject numerically weak pivots.
  std::vector<std::size_t> column_nnz(static_cast<std::size_t>(n_),0);
  for(Index i=0;i<n_;++i)
    for(const auto& [j,value] : u_[i])
      if(std::abs(value)>drop_tol) ++column_nnz[static_cast<std::size_t>(j)];

  constexpr Real markowitz_threshold=0.1;
  for(Index k=0;k<n_;++k) {
    Real column_scale=0;
    for(Index i=k;i<n_;++i) {
      auto it=u_[i].find(k);
      if(it!=u_[i].end())
        column_scale=std::max(column_scale,std::abs(it->second));
    }

    if(column_scale<=tol_) {
      n_=0;
      return false;
    }

    Index pivot=-1;
    std::size_t best_markowitz=std::numeric_limits<std::size_t>::max();
    Real best_abs=0;

    // Threshold partial pivoting: candidates must retain at least 10% of the
    // largest magnitude in the active pivot column. Among those candidates,
    // choose the smallest Markowitz product to control fill-in.
    const Real threshold=markowitz_threshold*column_scale;
    for(Index i=k;i<n_;++i) {
      auto it=u_[i].find(k);
      if(it==u_[i].end()) continue;
      const Real abs_value=std::abs(it->second);
      if(abs_value<threshold) continue;

      const std::size_t row_nnz=u_[i].size();
      const std::size_t col_nnz=column_nnz[static_cast<std::size_t>(k)];
      const std::size_t markowitz=(row_nnz>0?row_nnz-1:0)*
                                  (col_nnz>0?col_nnz-1:0);
      if(pivot<0 || markowitz<best_markowitz ||
         (markowitz==best_markowitz && abs_value>best_abs)) {
        pivot=i;
        best_markowitz=markowitz;
        best_abs=abs_value;
      }
    }

    // Fall back to the strongest available pivot if the threshold rejected
    // every candidate. This preserves the previous partial-pivoting behavior
    // for difficult numerical bases.
    if(pivot<0) {
      for(Index i=k;i<n_;++i) {
        auto it=u_[i].find(k);
        if(it!=u_[i].end() && (pivot<0 || std::abs(it->second)>best_abs)) {
          pivot=i;
          best_abs=std::abs(it->second);
        }
      }
    }

    if(pivot<0 || best_abs<=tol_) {
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
      --column_nnz[static_cast<std::size_t>(k)];
      if(std::abs(multiplier)>drop_tol) l_[i][k]=multiplier;

      // U[k] is sparse; propagate only its entries after the pivot.
      for(const auto& [j, value] : u_[k]) {
        if(j<=k) continue;
        auto existing=u_[i].find(j);
        const bool had_entry=existing!=u_[i].end();
        const Real updated=had_entry ? existing->second-multiplier*value
                                     : -multiplier*value;
        if(std::abs(updated)<=drop_tol) {
          if(had_entry) {
            u_[i].erase(existing);
            --column_nnz[static_cast<std::size_t>(j)];
          }
        } else {
          if(had_entry)
            existing->second=updated;
          else {
            u_[i][j]=updated;
            ++column_nnz[static_cast<std::size_t>(j)];
          }
        }
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
