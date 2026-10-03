#include "solver/simplex/primal_simplex.hpp"
#include <cmath>
#include <limits>
#include <algorithm>
namespace solver {
SolveResult PrimalSimplexSolver::solve(const LinearModel& m,std::size_t maxit) const {
 std::string err;if(!m.validate(err))return {SolveStatus::InvalidModel,0,{}, {},0,0,0,err};
 const Index n=m.variables.size(), rows=m.constraints.size();
 // Phase-1 foundation intentionally supports the canonical bounded subset: x>=0, Ax<=b, finite nonnegative upper bounds.
 std::vector<std::vector<Real>> A;std::vector<Real>b;std::vector<Real> c(n);
 for(Index j=0;j<n;++j){if(m.variables[j].lower_bound<-1e-12){return {SolveStatus::UnsupportedModel,0,{}, {},0,0,0,"Phase 1 primal simplex requires nonnegative variable lower bounds."};}c[j]=m.minimize?-m.variables[j].objective:m.variables[j].objective;if(m.variables[j].upper_bound<kInfinity){std::vector<Real> r(n,0);r[j]=1;A.push_back(r);b.push_back(m.variables[j].upper_bound);}}
 for(Index i=0;i<rows;++i){if(m.constraints[i].lower_bound>-kInfinity && std::abs(m.constraints[i].lower_bound-m.constraints[i].upper_bound)<1e-12)return {SolveStatus::UnsupportedModel,0,{}, {},0,0,0,"Equality constraints are reserved for the full Phase 2 simplex."};if(m.constraints[i].lower_bound>-kInfinity)return {SolveStatus::UnsupportedModel,0,{}, {},0,0,0,">= constraints are reserved for the full Phase 2 simplex."};if(m.constraints[i].upper_bound==kInfinity)continue;std::vector<Real> r(n);for(Index j=0;j<n;++j){std::vector<Real> unit(n,0);unit[j]=1;std::vector<Real> y;/* populated below */}
 }
 // Extract A columns into row form.
 for(Index i=0;i<rows;++i){if(m.constraints[i].upper_bound==kInfinity)continue;std::vector<Real> r(n,0);for(Index j=0;j<n;++j)for(Index p=m.A.column_pointers()[j];p<m.A.column_pointers()[j+1];++p)if(m.A.row_indices()[p]==i)r[j]=m.A.values()[p];A.push_back(std::move(r));b.push_back(m.constraints[i].upper_bound);}
 const Index M=A.size(), total=n+M;std::vector<std::vector<Real>> tab(M+1,std::vector<Real>(total+1,0));
 for(Index i=0;i<M;++i){if(b[i]<-1e-10)return {SolveStatus::Infeasible,0,{}, {},0,0,0,"negative RHS requires Phase 1 artificial-variable procedure."};for(Index j=0;j<n;++j)tab[i][j]=A[i][j];tab[i][n+i]=1;tab[i][total]=b[i];}
 for(Index j=0;j<n;++j)tab[M][j]=-c[j];
 std::vector<Index> basis(M);for(Index i=0;i<M;++i)basis[i]=n+i;
 std::size_t it=0;const Real tol=1e-9;
 while(it++<maxit){Index enter=-1;for(Index j=0;j<total;++j)if(tab[M][j]<-tol){enter=j;break;}if(enter<0){std::vector<Real>x(n,0);for(Index i=0;i<M;++i)if(basis[i]<n)x[basis[i]]=tab[i][total];Real z=m.objective_value(x);Real pres=0;std::vector<Real> Ax; m.A.multiply(x,Ax);for(Index i=0;i<rows;++i){if(m.constraints[i].upper_bound<kInfinity)pres=std::max(pres,Ax[i]-m.constraints[i].upper_bound);if(m.constraints[i].lower_bound>-kInfinity)pres=std::max(pres,m.constraints[i].lower_bound-Ax[i]);}return {SolveStatus::Optimal,z,x,{},std::max(0.0,pres),0,it-1,"Optimal basic solution found."};}
 Index leave=-1;Real best=std::numeric_limits<Real>::infinity();for(Index i=0;i<M;++i)if(tab[i][enter]>tol){Real ratio=tab[i][total]/tab[i][enter];if(ratio<best-tol){best=ratio;leave=i;}}
 if(leave<0)return {SolveStatus::Unbounded,0,{}, {},0,0,it-1,"Objective is unbounded in the Phase 1 canonical simplex form."};
 Real pivot=tab[leave][enter];for(Index j=0;j<=total;++j)tab[leave][j]/=pivot;for(Index i=0;i<=M;++i)if(i!=leave){Real q=tab[i][enter];if(std::abs(q)>tol)for(Index j=0;j<=total;++j)tab[i][j]-=q*tab[leave][j];}basis[leave]=enter;
 }
 return {SolveStatus::IterationLimit,0,{}, {},0,0,it,"Simplex iteration limit reached."};
}
}
