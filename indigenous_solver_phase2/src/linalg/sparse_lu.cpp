#include "solver/linalg/sparse_lu.hpp"
#include <algorithm>
#include <cmath>
#include <chrono>
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
        const Real current_pivot = [&]() {
          auto current = u_[pivot].find(k);
          return current==u_[pivot].end() ? Real(0) : current->second;
        }();
        if(std::abs(it->second)>std::abs(current_pivot))
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
        auto existing=u_[i].find(j);
        if(existing==u_[i].end()) {
          const Real updated=-multiplier*value;
          if(std::abs(updated)>drop_tol)
            u_[i][j]=updated;
        } else {
          const Real updated=existing->second-multiplier*value;
          if(std::abs(updated)<=drop_tol)
            u_[i].erase(existing);
          else
            existing->second=updated;
        }
      }
    }
  }

  return std::isfinite(min_pivot_);
}


bool SparseLU::factorize_sparse_columns(
    const std::vector<std::vector<std::pair<Index, Real>>>& columns,
    Index dimension, Real tol) {
  n_=dimension;
  tol_=std::max<Real>(tol, 0);
  const Real drop_tol=std::max<Real>(100*std::numeric_limits<Real>::epsilon(), tol_*1e-3);
  min_pivot_=std::numeric_limits<Real>::infinity();
  l_.clear();
  u_.clear();
  perm_.clear();
  etas_.clear();

  if(n_==0 || static_cast<Index>(columns.size())!=n_) {
    n_=0;
    return false;
  }
  u_.resize(n_);
  l_.resize(n_);
  perm_.resize(n_);

  // Populate U directly from the sparse basis columns. Unlike the dense
  // factorize() path, this avoids scanning every zero in the basis matrix.
  for(Index j=0;j<n_;++j) {
    for(const auto& [i,value] : columns[j]) {
      if(i>=n_) {
        n_=0;
        return false;
      }
      if(std::abs(value)>drop_tol) u_[i][j]=value;
    }
  }
  for(Index i=0;i<n_;++i) perm_[i]=i;

  for(Index k=0;k<n_;++k) {
    Index pivot=k;
    Real column_scale=0;
    for(Index i=k;i<n_;++i) {
      auto it=u_[i].find(k);
      if(it!=u_[i].end()) {
        column_scale=std::max(column_scale,std::abs(it->second));
        const Real current_pivot = [&]() {
          auto current = u_[pivot].find(k);
          return current==u_[pivot].end() ? Real(0) : current->second;
        }();
        if(std::abs(it->second)>std::abs(current_pivot))
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
    for(Index i=k+1;i<n_;++i) {
      auto col_it=u_[i].find(k);
      if(col_it==u_[i].end()) continue;
      const Real multiplier=col_it->second/pivot_value;
      u_[i].erase(col_it);
      if(std::abs(multiplier)>drop_tol) l_[i][k]=multiplier;
      for(const auto& [j, value] : u_[k]) {
        if(j<=k) continue;
        const Real updated=u_[i].count(j)?u_[i][j]-multiplier*value
                                        :-multiplier*value;
        if(std::abs(updated)<=drop_tol)
          u_[i].erase(j);
        else
          u_[i][j]=updated;
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

  // Reuse one working vector for P*b, forward substitution, and back
  // substitution. This avoids allocating/copying separate y and x vectors
  // on every FTRAN.
  x.resize(n_);
  for(Index i=0;i<n_;++i) {
    x[i]=b[perm_[i]];
    for(const auto& [j,value] : l_[i])
      if(j<i) x[i]-=value*x[j];
  }

  // U*x=y.
  for(Index ii=n_; ii-->0; ) {
    Real rhs=x[ii];
    for(const auto& [j,value] : u_[ii])
      if(j>ii) rhs-=value*x[j];

    auto diag=u_[ii].find(ii);
    if(diag==u_[ii].end() || std::abs(diag->second)<=tol_) return false;
    x[ii]=rhs/diag->second;
  }

  // B_current = B_base * E_1 * ... * E_k. For an eta matrix E whose
  // replacement column is d, E*x=y gives x[r]=y[r]/d[r] and
  // x[i]=y[i]-d[i]*x[r]. Apply E_1^{-1}, then E_2^{-1}, ... in order.
  const auto eta_start=std::chrono::steady_clock::now();
  for(const auto& eta:etas_) {
    const Real pivot_component=x[eta.pivot_row]/eta.pivot;
    for(Index i=0;i<n_;++i) if(i!=eta.pivot_row)
      x[i]-=eta.direction[i]*pivot_component;
    x[eta.pivot_row]=pivot_component;
  }
  last_eta_forward_ms_=std::chrono::duration<double,std::milli>(
      std::chrono::steady_clock::now()-eta_start).count();
  return true;
}

bool SparseLU::solve_transpose(const std::vector<Real>& b,
                               std::vector<Real>& x) const {
  if(n_==0 || static_cast<Index>(b.size())!=n_) return false;

  // A^T*x=b and P*A=L*U imply:
  // U^T*y=b, L^T*z=y, P*x=z.
  // B_current^T = E_k^T ... E_1^T B_base^T. Solve the eta system first,
  // in reverse update order, then solve the base transpose factors.
  // Reuse x as the working vector for the eta, U^T, and L^T solves.
  // This avoids three temporary vector allocations/copies on every BTRAN.
  x=b;
  const auto eta_start=std::chrono::steady_clock::now();
  for(auto it=etas_.rbegin();it!=etas_.rend();++it) {
    const auto& eta=*it;
    Real sum=0;
    for(Index i=0;i<n_;++i) if(i!=eta.pivot_row)
      sum+=eta.direction[i]*x[i];
    x[eta.pivot_row]=(x[eta.pivot_row]-sum)/eta.pivot;
  }
  last_eta_transpose_ms_=std::chrono::duration<double,std::milli>(
      std::chrono::steady_clock::now()-eta_start).count();

  // U^T*x=x. Scatter each solved component through the existing sparse row
  // entries instead of scanning all preceding columns.
  for(Index i=0;i<n_;++i) {
    auto diag=u_[i].find(i);
    if(diag==u_[i].end() || std::abs(diag->second)<=tol_) return false;
    x[i]/=diag->second;
    for(const auto& [j,value] : u_[i]) {
      if(j>i) x[j]-=value*x[i];
    }
  }

  // L^T*x=x. L has an implicit unit diagonal.
  for(Index i=n_; i-->0; ) {
    for(const auto& [j,value] : l_[i]) {
      if(j<i) x[j]-=value*x[i];
    }
  }

  // Undo the row permutation.
  std::vector<Real> permuted=x;
  for(Index i=0;i<n_;++i) x[perm_[i]]=permuted[i];
  return true;
}

} // namespace solver
