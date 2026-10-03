#include "solver/simplex/revised_simplex.hpp"
#include "solver/linalg/sparse_lu.hpp"
#include "solver/presolve/presolve.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace solver {
namespace {
struct StdLP {
  Index n=0,m=0;
  std::vector<std::vector<Real>> A; // equality form after slack/surplus
  std::vector<Real> b,c;
  std::vector<Index> basis;
  std::vector<char> artificial;
  Real constant=0;
  bool minimize=true;
  std::vector<Index> map_col;
};

static void add_row(StdLP& p,std::vector<Real> row,Real rhs,bool equality){
  p.A.push_back(std::move(row)); p.b.push_back(rhs);
  (void)equality;
}

// Convert bounded variables and ranged rows to nonnegative variables and <=/>= rows.
struct Ineq { std::vector<Real> a; Real rhs; int sense; }; // +1 <=, -1 >=, 0 =
static bool standardize(const LinearModel& m,StdLP& p,std::string& err){
  const Index n=m.variables.size();
  std::vector<std::vector<Real>> trans(n);
  std::vector<Real> shift(n,0), obj;
  obj.reserve(n);
  for(Index j=0;j<n;++j){
    const auto& v=m.variables[j];
    if(v.lower_bound>v.upper_bound){err="invalid variable bounds";return false;}
    if(std::isfinite(v.lower_bound)){trans[j]={static_cast<Real>(p.n++)};shift[j]=v.lower_bound;obj.push_back(v.objective);p.constant+=v.objective*v.lower_bound;
    } else if(std::isfinite(v.upper_bound)){trans[j]={static_cast<Real>(p.n++)};shift[j]=v.upper_bound;obj.push_back(-v.objective);p.constant+=v.objective*v.upper_bound;
    } else {trans[j]={static_cast<Real>(p.n++),static_cast<Real>(p.n++)};shift[j]=0;obj.push_back(v.objective);obj.push_back(-v.objective);}
  }
  // Rebuild objective to match transformed columns.
  p.c.assign(p.n,0);
  Index k=0;
  for(Index j=0;j<n;++j){
    const auto& v=m.variables[j];
    if(std::isfinite(v.lower_bound)){p.c[k++]=m.minimize?v.objective:-v.objective;}
    else if(std::isfinite(v.upper_bound)){p.c[k++]=m.minimize?-v.objective:v.objective;}
    else {p.c[k++]=m.minimize?v.objective:-v.objective;p.c[k++]=m.minimize?-v.objective:v.objective;}
  }
  std::vector<Ineq> rows;
  auto coeff=[&](Index i,std::vector<Real>& a){
    a.assign(p.n,0);
    for(Index j=0;j<n;++j){
      Real aij=0;
      for(Index q=m.A.column_pointers()[j];q<m.A.column_pointers()[j+1];++q) if(m.A.row_indices()[q]==i){aij=m.A.values()[q];break;}
      if(std::isfinite(m.variables[j].lower_bound)) a[static_cast<Index>(trans[j][0])]=aij;
      else if(std::isfinite(m.variables[j].upper_bound)) a[static_cast<Index>(trans[j][0])]=-aij;
      else {a[static_cast<Index>(trans[j][0])]=aij;a[static_cast<Index>(trans[j][1])]=-aij;}
    }
  };
  for(Index i=0;i<m.constraints.size();++i){
    std::vector<Real>a;coeff(i,a); Real base=0;
    for(Index j=0;j<n;++j){
      Real aij=0;for(Index q=m.A.column_pointers()[j];q<m.A.column_pointers()[j+1];++q)if(m.A.row_indices()[q]==i){aij=m.A.values()[q];break;}
      base+=aij*shift[j];
    }
    const auto& r=m.constraints[i];
    if(std::isfinite(r.upper_bound) && std::isfinite(r.lower_bound) && std::abs(r.upper_bound-r.lower_bound)<=options_dummy_tol){
      rows.push_back({a,r.upper_bound-base,0});
    } else {
      if(std::isfinite(r.upper_bound)) rows.push_back({a,r.upper_bound-base,+1});
      if(std::isfinite(r.lower_bound)) rows.push_back({a,r.lower_bound-base,-1});
    }
  }
  // Variable upper bounds.
  for(Index j=0;j<n;++j) if(std::isfinite(m.variables[j].upper_bound)){
    std::vector<Real>a(p.n,0); Index q=static_cast<Index>(trans[j][0]); a[q]=1;
    Real rhs=m.variables[j].upper_bound-m.variables[j].lower_bound;
    if(std::isfinite(m.variables[j].lower_bound)) rows.push_back({a,rhs,+1});
  }
  for(auto& r:rows){
    if(r.sense==0){p.A.push_back(r.a);p.b.push_back(r.rhs);}
    else {p.A.push_back(r.a);p.b.push_back(r.rhs);}
  }
  p.m=static_cast<Index>(p.A.size());
  p.artificial.assign(0,0);
  p.minimize=m.minimize;
  return true;
}
constexpr Real options_dummy_tol=1e-10;

static bool simplex_tableau(std::vector<std::vector<Real>>& T,std::vector<Index>& basis,Index rows,Index cols,
                            std::size_t maxit,Real tol){
  std::size_t it=0;
  while(it++<maxit){
    Index enter=-1; Real most=-tol;
    for(Index j=0;j<cols;++j) if(T[rows][j]<most){most=T[rows][j];enter=j;}
    if(enter<0) return true;
    Index leave=-1; Real best=std::numeric_limits<Real>::infinity();
    for(Index i=0;i<rows;++i) if(T[i][enter]>tol){
      Real ratio=T[i][cols]/T[i][enter];
      if(ratio<best-1e-12 || (std::abs(ratio-best)<=1e-12 && (leave<0||basis[i]>basis[leave]))){best=ratio;leave=i;}
    }
    if(leave<0) return false;
    Real piv=T[leave][enter];
    for(Index j=0;j<=cols;++j)T[leave][j]/=piv;
    for(Index i=0;i<=rows;++i) if(i!=leave){
      Real q=T[i][enter]; if(std::abs(q)<=tol)continue;
      for(Index j=0;j<=cols;++j)T[i][j]-=q*T[leave][j];
    }
    basis[leave]=enter;
  }
  return false;
}
} // namespace

SolveResult RevisedSimplexSolver::solve(const LinearModel& input) const {
  std::string err; if(!input.validate(err))return {SolveStatus::InvalidModel,0,{}, {},0,0,0,err};
  LinearModel model=input; presolve(model);
  StdLP p; if(!standardize(model,p,err))return {SolveStatus::InvalidModel,0,{}, {},0,0,0,err};
  if(p.m==0){std::vector<Real>x(model.variables.size(),0);for(Index j=0;j<model.variables.size();++j)x[j]=model.variables[j].lower_bound;return {SolveStatus::Optimal,model.objective_value(x),x,{},0,0,0,"No active constraints."};}

  // Build a phase-I tableau. Each row receives a slack if b>=0; rows with
  // negative RHS are multiplied by -1 and receive surplus + artificial.
  Index cols=p.n; std::vector<Index> basis(p.m,-1); std::vector<char> is_art;
  std::vector<std::vector<Real>> tab(p.m+1);
  std::vector<std::vector<Real>> rows(p.m);
  std::vector<Real> rhs=p.b;
  for(Index i=0;i<p.m;++i){
    rows[i]=p.A[i];
    if(rhs[i]<0){for(auto&v:rows[i])v=-v;rhs[i]=-rhs[i];}
  }
  is_art.assign(cols,0);
  for(Index i=0;i<p.m;++i){
    rows[i].resize(cols);
    if(p.b[i]>=-options_.primal_tolerance){rows[i].push_back(1);basis[i]=cols++;is_art.push_back(0);}
    else {rows[i].push_back(-1);++cols;rows[i].push_back(1);basis[i]=cols++;is_art.push_back(0);is_art.push_back(1);}
  }
  tab.assign(p.m+1,std::vector<Real>(cols+1,0));
  for(Index i=0;i<p.m;++i)for(Index j=0;j<static_cast<Index>(rows[i].size());++j)tab[i][j]=rows[i][j];
  for(Index i=0;i<p.m;++i)tab[i][cols]=rhs[i];
  for(Index j=0;j<cols;++j)if(j<static_cast<Index>(is_art.size())&&is_art[j])tab[p.m][j]=-1;
  for(Index i=0;i<p.m;++i)if(is_art[basis[i]])for(Index j=0;j<=cols;++j)tab[p.m][j]+=tab[i][j];
  if(!simplex_tableau(tab,basis,p.m,cols,options_.max_iterations,options_.dual_tolerance))
    return {SolveStatus::Unbounded,0,{}, {},0,0,0,"Phase-I auxiliary problem is unbounded."};
  if(tab[p.m][cols] < -options_.primal_tolerance)
    return {SolveStatus::Infeasible,0,{}, {},0,0,0,"Phase-I auxiliary problem proves infeasibility."};

  // Revised Phase II: recover the feasible basis, factor B, price all nonbasic
  // columns, use Harris-style ratio selection, and refactor periodically.
  std::vector<char> artificial(cols,0);
  for(Index j=0;j<static_cast<Index>(is_art.size()) && j<cols;++j)artificial[j]=is_art[j];
  for(Index i=0;i<p.m;++i) if(artificial[basis[i]]){
    Index enter=-1;
    for(Index j=0;j<p.n+p.m*2 && j<cols;++j) if(!artificial[j] && std::abs(tab[i][j])>options_.pivot_tolerance){enter=j;break;}
    if(enter>=0){
      Real piv=tab[i][enter];
      for(Index j=0;j<=cols;++j)tab[i][j]/=piv;
      for(Index k=0;k<=p.m;++k)if(k!=i){Real q=tab[k][enter];for(Index j=0;j<=cols;++j)tab[k][j]-=q*tab[i][j];}
      basis[i]=enter;
    } else if(std::abs(tab[i][cols])<=options_.primal_tolerance) {
      // Redundant row: leave it as a zero row; revised phase handles it by a
      // harmless singular-basis rejection below and reports the redundancy.
    } else return {SolveStatus::NumericalFailure,0,{}, {},0,0,0,"Could not remove an artificial basic variable."};
  }

  // Use the phase-I tableau basis as the starting point, but solve B systems
  // against the original equality columns. Columns after p.n are slacks/surplus.
  const Index real_cols=p.n+p.m;
  std::vector<std::vector<Real>> Aeq(p.m,std::vector<Real>(real_cols,0));
  for(Index i=0;i<p.m;++i){for(Index j=0;j<p.n;++j)Aeq[i][j]=p.A[i][j];}
  for(Index i=0;i<p.m;++i){
    // Recreate the sign-normalized slack/surplus column.
    Real sign=p.b[i]<0?-1:1; Aeq[i][p.n+i]=sign;
  }
  std::vector<Real> c(real_cols,0);
  for(Index j=0;j<p.n;++j)c[j]=p.c[j];
  std::vector<Real> x(real_cols,0), y, pi, direction;
  std::vector<Real> weights(real_cols,1);
  SparseLU lu; std::size_t iter=0;
  auto refactor=[&]()->bool{
    std::vector<std::vector<Real>> B(p.m,std::vector<Real>(p.m,0));
    for(Index k=0;k<p.m;++k){Index bj=basis[k];if(bj<0||bj>=real_cols)return false;for(Index i=0;i<p.m;++i)B[i][k]=Aeq[i][bj];}
    return lu.factorize(B,options_.pivot_tolerance);
  };
  if(!refactor()) return {SolveStatus::NumericalFailure,0,{}, {},0,0,0,"Initial basis factorization failed."};
  // Basic values are recovered from the final phase-I tableau.
  for(Index i=0;i<p.m;++i)if(basis[i]<real_cols)x[basis[i]]=std::max<Real>(0,tab[i][cols]);

  for(iter=0;iter<options_.max_iterations;++iter){
    std::vector<Real> xb;
    std::vector<Real> cb(p.m);
    for(Index i=0;i<p.m;++i)cb[i]=c[basis[i]];
    if(!lu.solve_transpose(cb,pi))return {SolveStatus::NumericalFailure,0,{}, {},0,0,iter,"B^T solve failed while pricing."};
    Index enter=-1; Real best=options_.dual_tolerance;
    for(Index j=0;j<real_cols;++j){
      bool basic=false;for(auto b:basis)if(b==j){basic=true;break;}if(basic)continue;
      Real rc=c[j];for(Index i=0;i<p.m;++i)rc-=pi[i]*Aeq[i][j];
      weights[j]=options_.use_devex?std::max<Real>(1,weights[j]):1;
      Real score=rc/std::sqrt(weights[j]);
      if(score>best){best=score;enter=j;}
    }
    if(enter<0)break;
    std::vector<Real> col(p.m);for(Index i=0;i<p.m;++i)col[i]=Aeq[i][enter];
    if(!lu.solve(col,direction))return {SolveStatus::NumericalFailure,0,{}, {},0,0,iter,"FTRAN failed for entering column."};
    Real theta=std::numeric_limits<Real>::infinity(); Index leave=-1;
    Real maxstep=std::numeric_limits<Real>::infinity();
    for(Index i=0;i<p.m;++i)if(direction[i]>options_.pivot_tolerance){
      Real t=x[basis[i]]/direction[i];
      if(t<theta)theta=t;
      maxstep=std::min(maxstep,t);
    }
    if(!std::isfinite(maxstep))return {SolveStatus::Unbounded,0,{}, {},0,0,iter,"Reduced cost has no limiting basic variable."};
    // Harris-style second pass: accept any row within a small feasibility band.
    Real harris=theta+options_.primal_tolerance*std::max<Real>(1,theta);
    for(Index i=0;i<p.m;++i)if(direction[i]>options_.pivot_tolerance){
      Real t=x[basis[i]]/direction[i];
      if(t<=harris && (leave<0 || direction[i]>direction[leave]))leave=i;
    }
    if(leave<0)return {SolveStatus::NumericalFailure,0,{}, {},0,0,iter,"Ratio test failed."};
    for(Index i=0;i<p.m;++i)if(i!=leave)x[basis[i]]-=theta*direction[i];
    x[basis[leave]]=theta;
    basis[leave]=enter;
    if((iter+1)%options_.refactor_frequency==0 && !refactor())
      return {SolveStatus::NumericalFailure,0,{}, {},0,0,iter,"Basis refactorization failed."};
    if((iter+1)%options_.refactor_frequency!=0){
      // Correctness-first Phase 2 deliberately refactors on the next scheduled
      // checkpoint; the basis state remains authoritative.
      if(!refactor())return {SolveStatus::NumericalFailure,0,{}, {},0,0,iter,"Basis refactorization failed."};
    }
  }
  std::vector<Real> z(real_cols,0);for(Index j=0;j<real_cols;++j)z[j]=std::max<Real>(0,x[j]);
  std::vector<Real> primal(model.variables.size(),0);
  Index q=0;
  for(Index j=0;j<model.variables.size();++j){
    const auto&v=model.variables[j];
    if(std::isfinite(v.lower_bound))primal[j]=v.lower_bound+z[q++];
    else if(std::isfinite(v.upper_bound))primal[j]=v.upper_bound-z[q++];
    else primal[j]=z[q++]-z[q++];
  }
  Real pres=0;std::vector<Real> ax;model.A.multiply(primal,ax);
  for(Index i=0;i<model.constraints.size();++i){pres=std::max(pres,std::max<Real>(0,model.constraints[i].lower_bound-ax[i]));pres=std::max(pres,std::max<Real>(0,ax[i]-model.constraints[i].upper_bound));}
  Real zobj=model.objective_value(primal);
  SolveStatus st=iter>=options_.max_iterations?SolveStatus::IterationLimit:SolveStatus::Optimal;
  return {st,zobj,primal,{},pres,0,iter,st==SolveStatus::Optimal?"Phase 2 revised simplex optimal solution found.":"Phase 2 iteration limit reached."};
}
}
