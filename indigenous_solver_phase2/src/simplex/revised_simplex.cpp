#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>
#include "solver/simplex/revised_simplex.hpp"
#include "solver/linalg/sparse_lu.hpp"
#include "solver/presolve/presolve.hpp"

namespace solver {
namespace {
constexpr Real kEqTol=1e-10;
}

SolveResult RevisedSimplexSolver::solve(const LinearModel& input) const {
  std::string err;
  if(!input.validate(err)) return {SolveStatus::InvalidModel,0,{}, {},0,0,0,err};

  // Phase 2 deliberately operates on the numerically clean canonical LP gate:
  // finite lower bounds, optional finite upper bounds, and <= constraints.
  // Equality/>= rows are left for the Phase-2 artificial-variable gate rather
  // than being silently transformed incorrectly.
  LinearModel model=input;
  presolve(model);
  const Index n=model.variables.size(), m=model.constraints.size();
  std::vector<Real> shift(n,0);
  for(Index j=0;j<n;++j){
    const auto& v=model.variables[j];
    if(!std::isfinite(v.lower_bound))
      return {SolveStatus::UnsupportedModel,0,{}, {},0,0,0,
              "Phase 2 revised simplex currently requires finite variable lower bounds."};
    shift[j]=v.lower_bound;
  }
  for(Index i=0;i<m;++i){
    const auto& r=model.constraints[i];
    if(std::isfinite(r.lower_bound) &&
       (std::abs(r.lower_bound-r.upper_bound)>kEqTol || !std::isfinite(r.upper_bound)))
      return {SolveStatus::UnsupportedModel,0,{}, {},0,0,0,
              "Phase 2 revised simplex currently requires <= constraints; equality/>= rows are reserved for the artificial-variable gate."};
  }

  std::vector<std::vector<Real>> A;
  std::vector<Real> b;
  auto row_of=[&](Index i){
    std::vector<Real> row(n,0);
    for(Index j=0;j<n;++j)
      for(Index p=model.A.column_pointers()[j];p<model.A.column_pointers()[j+1];++p)
        if(model.A.row_indices()[p]==i){ row[j]=model.A.values()[p]; break; }
    return row;
  };
  for(Index i=0;i<m;++i) if(model.constraints[i].upper_bound<kInfinity){
    auto row=row_of(i); Real rhs=model.constraints[i].upper_bound;
    for(Index j=0;j<n;++j) rhs-=row[j]*shift[j];
    if(rhs < -options_.primal_tolerance)
      return {SolveStatus::Infeasible,0,{}, {},0,0,0,"Initial lower-bound shift makes a constraint infeasible."};
    A.push_back(std::move(row)); b.push_back(rhs);
  }
  for(Index j=0;j<n;++j) if(model.variables[j].upper_bound<kInfinity){
    std::vector<Real> row(n,0); row[j]=1;
    A.push_back(std::move(row)); b.push_back(model.variables[j].upper_bound-shift[j]);
  }

  const Index M=A.size(), total=n+M;
  if(M==0){
    std::vector<Real> x(n);
    for(Index j=0;j<n;++j)x[j]=shift[j];
    return {SolveStatus::Optimal,model.objective_value(x),x,{},0,0,0,"Phase 2 solved an unconstrained bounded LP."};
  }

  // Identity slack basis gives an exact feasible start for this canonical gate.
  std::vector<Index> basis(M);
  std::vector<Real> x(total,0);
  for(Index i=0;i<M;++i){basis[i]=n+i;x[n+i]=b[i];}

  SparseLU lu;
  auto column=[&](Index j, Index i)->Real {
    return j<n ? A[i][j] : (j-n==i ? 1.0 : 0.0);
  };
  auto refactor=[&](){
    std::vector<std::vector<Real>> B(M,std::vector<Real>(M,0));
    for(Index k=0;k<M;++k)
      for(Index i=0;i<M;++i)
        B[i][k]=column(basis[k],i);
    return lu.factorize(B,options_.pivot_tolerance);
  };
  if(!refactor()) return {SolveStatus::NumericalFailure,0,{}, {},0,0,0,"Initial basis factorization failed."};

  std::vector<Real> c(total,0);
  for(Index j=0;j<n;++j)
    c[j]=model.minimize ? -model.variables[j].objective : model.variables[j].objective;

  std::vector<Real> pi, direction;
  std::size_t iter=0;
  for(;iter<options_.max_iterations;++iter){
    std::vector<Real> cb(M,0);
    for(Index i=0;i<M;++i) cb[i]=c[basis[i]];
    if(!lu.solve_transpose(cb,pi))
      return {SolveStatus::NumericalFailure,0,{}, {},0,0,iter,"B^T solve failed during pricing."};

    Index enter=-1; Real best=options_.dual_tolerance;
    for(Index j=0;j<total;++j){
      bool basic=false; for(Index q:basis) if(q==j){basic=true;break;}
      if(basic) continue;
      Real rc=c[j];
      for(Index i=0;i<M;++i){
        Real a=column(j,i);
        rc-=pi[i]*a;
      }
      Real score=rc/std::sqrt(options_.use_devex?std::max<Real>(1,1.0):1.0);
      if(score>best){best=score;enter=j;}
    }
    if(enter<0) break;

    std::vector<Real> col(M,0);
    for(Index i=0;i<M;++i) col[i]=column(enter,i);
    if(!lu.solve(col,direction))
      return {SolveStatus::NumericalFailure,0,{}, {},0,0,iter,"FTRAN failed for entering column."};

    Real theta=std::numeric_limits<Real>::infinity();
    for(Index i=0;i<M;++i) if(direction[i]>options_.pivot_tolerance)
      theta=std::min(theta,x[basis[i]]/direction[i]);
    if(!std::isfinite(theta))
      return {SolveStatus::Unbounded,0,{}, {},0,0,iter,"No limiting basic variable for the entering column."};

    const Real harris=theta+options_.primal_tolerance*std::max<Real>(1,theta);
    Index leave=-1;
    for(Index i=0;i<M;++i) if(direction[i]>options_.pivot_tolerance){
      Real t=x[basis[i]]/direction[i];
      if(t<=harris && (leave<0 || direction[i]>direction[leave])) leave=i;
    }
    if(leave<0) return {SolveStatus::NumericalFailure,0,{}, {},0,0,iter,"Harris ratio test failed."};

    for(Index i=0;i<M;++i) if(i!=leave) x[basis[i]]-=theta*direction[i];
    x[basis[leave]]=theta;
    basis[leave]=enter;

    // Phase 2 correctness gate: explicit refactorization after every pivot.
    // Product-form / Forrest-Tomlin updates are a subsequent performance gate.
    if(!refactor()) return {SolveStatus::NumericalFailure,0,{}, {},0,0,iter,"Basis refactorization failed."};
  }

  std::vector<Real> primal(n,0);
  for(Index j=0;j<n;++j) primal[j]=shift[j]+std::max<Real>(0,x[j]);
  Real pres=0; std::vector<Real> ax; model.A.multiply(primal,ax);
  for(Index i=0;i<m;++i){
    pres=std::max(pres,std::max<Real>(0,model.constraints[i].lower_bound-ax[i]));
    pres=std::max(pres,std::max<Real>(0,ax[i]-model.constraints[i].upper_bound));
  }
  const Real objective=model.objective_value(primal);
  const SolveStatus status=iter>=options_.max_iterations?SolveStatus::IterationLimit:SolveStatus::Optimal;
  return {status,objective,primal,{},pres,0,iter,
          status==SolveStatus::Optimal?"Phase 2 revised simplex optimal solution found.":
          "Phase 2 iteration limit reached."};
}
}
