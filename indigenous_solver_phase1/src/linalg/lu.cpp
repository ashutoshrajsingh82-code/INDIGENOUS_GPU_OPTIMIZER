#include "solver/linalg/lu.hpp"
#include <cmath>
namespace solver {
bool LUFactorization::factorize(const std::vector<std::vector<Real>>& A,Real tol){n_=A.size();tol_=tol;if(n_==0)return false;for(auto&r:A)if((Index)r.size()!=n_)return false;lu_=A;piv_.resize(n_);for(Index i=0;i<n_;++i)piv_[i]=i;for(Index k=0;k<n_;++k){Index p=k;Real best=std::abs(lu_[k][k]);for(Index i=k+1;i<n_;++i)if(std::abs(lu_[i][k])>best){best=std::abs(lu_[i][k]);p=i;}if(best<=tol_)return false;if(p!=k){std::swap(lu_[p],lu_[k]);std::swap(piv_[p],piv_[k]);}for(Index i=k+1;i<n_;++i){lu_[i][k]/=lu_[k][k];for(Index j=k+1;j<n_;++j)lu_[i][j]-=lu_[i][k]*lu_[k][j];}}return true;}
bool LUFactorization::solve(const std::vector<Real>& b,std::vector<Real>& x) const {if((Index)b.size()!=n_||n_==0)return false;x=b;std::vector<Real> pb(n_);for(Index i=0;i<n_;++i)pb[i]=b[piv_[i]];x=pb;for(Index i=0;i<n_;++i)for(Index j=0;j<i;++j)x[i]-=lu_[i][j]*x[j];for(Index i=n_-1;i>=0;--i){for(Index j=i+1;j<n_;++j)x[i]-=lu_[i][j]*x[j];if(std::abs(lu_[i][i])<=tol_)return false;x[i]/=lu_[i][i];}return true;}
}
