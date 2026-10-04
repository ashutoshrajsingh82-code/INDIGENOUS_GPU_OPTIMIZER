#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <cstdlib>
#include <iostream>
#include <vector>
#include "solver/simplex/revised_simplex.hpp"
#include "solver/linalg/sparse_lu.hpp"
#include "solver/presolve/presolve.hpp"

namespace solver {
namespace {
constexpr Real kEqTol=1e-10;

struct StandardRow {
  std::vector<Real> a;
  Real rhs=0;
  int sense=0; // -1 <=, 0 =, +1 >=
};

struct StandardSystem {
  std::vector<std::vector<Real>> A;
  std::vector<Real> b;
  std::vector<Index> basis;
  std::vector<bool> artificial;
};

static bool nearly_equal(Real a, Real b, Real tol) {
  return std::abs(a-b) <= tol*std::max<Real>(1.0,std::max(std::abs(a),std::abs(b)));
}
}

SolveResult RevisedSimplexSolver::solve(const LinearModel& input) const {
  std::string err;
  if(!input.validate(err)) return {SolveStatus::InvalidModel,0,{}, {},0,0,0,err};

  LinearModel model=input;
  presolve(model);
  const Index n=model.variables.size(), m=model.constraints.size();

  std::vector<Real> shift(n,0);
  for(Index j=0;j<n;++j){
    const auto& v=model.variables[j];
    if(!std::isfinite(v.lower_bound))
      return {SolveStatus::UnsupportedModel,0,{}, {},0,0,0,
              "Phase 2 revised simplex requires finite variable lower bounds."};
    shift[j]=v.lower_bound;
  }

  auto row_of=[&](Index i){
    std::vector<Real> row(n,0);
    for(Index j=0;j<n;++j)
      for(Index p=model.A.column_pointers()[j];p<model.A.column_pointers()[j+1];++p)
        if(model.A.row_indices()[p]==i){ row[j]=model.A.values()[p]; break; }
    return row;
  };

  // Convert all finite row bounds into <=, =, or >= rows after shifting
  // variables to nonnegative coordinates. Ranged rows become two inequalities.
  std::vector<StandardRow> rows;
  for(Index i=0;i<m;++i){
    const auto& r=model.constraints[i];
    auto a=row_of(i);
    Real lo=r.lower_bound, hi=r.upper_bound;
    if(std::isfinite(lo) && std::isfinite(hi)){
      if(nearly_equal(lo,hi,kEqTol)){
        StandardRow sr{a,hi,0};
        for(Index j=0;j<n;++j) sr.rhs-=a[j]*shift[j];
        rows.push_back(std::move(sr));
      } else {
        StandardRow up{a,hi,-1};
        for(Index j=0;j<n;++j) up.rhs-=a[j]*shift[j];
        rows.push_back(std::move(up));
        StandardRow low{a,lo,1};
        for(Index j=0;j<n;++j) low.rhs-=a[j]*shift[j];
        rows.push_back(std::move(low));
      }
    } else if(std::isfinite(hi)){
      StandardRow sr{a,hi,-1};
      for(Index j=0;j<n;++j) sr.rhs-=a[j]*shift[j];
      rows.push_back(std::move(sr));
    } else if(std::isfinite(lo)){
      StandardRow sr{a,lo,1};
      for(Index j=0;j<n;++j) sr.rhs-=a[j]*shift[j];
      rows.push_back(std::move(sr));
    } else {
      // Free row: no restriction.
    }
  }

  // Finite variable upper bounds are ordinary <= rows after the lower-bound shift.
  for(Index j=0;j<n;++j) if(model.variables[j].upper_bound<kInfinity){
    StandardRow sr;
    sr.a.assign(n,0); sr.a[j]=1;
    sr.rhs=model.variables[j].upper_bound-shift[j];
    sr.sense=-1;
    rows.push_back(std::move(sr));
  }

  // Normalize every row so that its RHS is nonnegative. Flipping a row also
  // flips its inequality direction. Equality rows remain equalities.
  for(auto& row:rows){
    if(row.rhs < -options_.primal_tolerance){
      row.rhs=-row.rhs;
      for(Real& v:row.a) v=-v;
      row.sense=-row.sense;
    } else if(std::abs(row.rhs)<=options_.primal_tolerance){
      row.rhs=0;
    }
  }

  // Build standard form A z = b, z >= 0. <= rows get slacks, >= rows get
  // surplus plus an artificial variable, and equality rows get an artificial
  // variable. The resulting basis is immediately feasible for Phase I.
  StandardSystem sys;
  const Index M=rows.size();
  sys.A.resize(M);
  sys.b.resize(M);
  std::vector<int> kind(M,0); // 0 slack, 1 artificial, 2 surplus+artificial
  Index artificial_count=0;
  for(Index i=0;i<M;++i){
    sys.A[i]=rows[i].a;
    sys.b[i]=rows[i].rhs;
    kind[i]=(rows[i].sense<0)?0:1;
    if(rows[i].sense>0) ++artificial_count;
    else if(rows[i].sense==0) ++artificial_count;
  }

  Index total=n;
  for(Index i=0;i<M;++i){
    if(rows[i].sense<0){
      ++total; // slack
    } else if(rows[i].sense>0){
      ++total; ++total; // surplus + artificial
    } else {
      ++total; // artificial
    }
  }
  if(M==0){
    std::vector<Real> primal=shift;
    return {SolveStatus::Optimal,model.objective_value(primal),primal,{},0,0,0,
            "Phase 2 solved an unconstrained bounded LP."};
  }

  for(auto& row:sys.A) row.resize(total,0);
  sys.b.resize(M);
  sys.b.assign(M,0);
  for(Index i=0;i<M;++i) sys.b[i]=rows[i].rhs;
  sys.basis.resize(M);
  sys.artificial.assign(total,false);

  Index next=n;
  for(Index i=0;i<M;++i){
    if(rows[i].sense<0){
      sys.A[i][next]=1;
      sys.basis[i]=next++;
    } else if(rows[i].sense>0){
      sys.A[i][next]=-1; // surplus
      ++next;
      sys.A[i][next]=1;  // artificial
      sys.artificial[next]=true;
      sys.basis[i]=next++;
    } else {
      sys.A[i][next]=1;
      sys.artificial[next]=true;
      sys.basis[i]=next++;
    }
  }

  SparseLU lu;
  auto column=[&](Index j, Index i)->Real { return sys.A[i][j]; };
  auto refactor=[&](){
    std::vector<std::vector<Real>> B(M,std::vector<Real>(M,0));
    for(Index k=0;k<M;++k)
      for(Index i=0;i<M;++i)
        B[i][k]=column(sys.basis[k],i);
    return lu.factorize(B,options_.pivot_tolerance);
  };

  if(!refactor())
    return {SolveStatus::NumericalFailure,0,{}, {},0,0,0,
            "Initial Phase I basis factorization failed."};

  std::vector<Real> x(total,0);
  for(Index i=0;i<M;++i) x[sys.basis[i]]=sys.b[i];

  std::vector<Real> devex_weight(total,1.0);
  std::vector<Real> pi, direction;
  std::size_t iterations=0;

  auto simplex_phase = [&](const std::vector<Real>& c, bool phase_one,
                           std::size_t& phase_iterations)->SolveStatus {
    phase_iterations=0;
    for(;phase_iterations<options_.max_iterations && iterations<options_.max_iterations;
        ++phase_iterations,++iterations){
      std::vector<Real> cb(M,0);
      for(Index i=0;i<M;++i) cb[i]=c[sys.basis[i]];
      if(!lu.solve_transpose(cb,pi)) return SolveStatus::NumericalFailure;

      Index enter=-1; Real best=options_.dual_tolerance;
      Real max_rc=-std::numeric_limits<Real>::infinity();
      Index max_rc_j=-1;
      std::size_t artificial_basic_count=0, nonzero_cost_count=0;
      for(Index q:sys.basis) if(sys.artificial[q]) ++artificial_basic_count;
      for(Index j=0;j<total;++j)
        if(std::abs(c[j])>options_.dual_tolerance) ++nonzero_cost_count;
      for(Index j=0;j<total;++j){
        bool basic=false;
        for(Index q:sys.basis) if(q==j){basic=true;break;}
        if(basic) continue;
        if(!phase_one && sys.artificial[j]) continue;

        Real rc=c[j];
        for(Index i=0;i<M;++i) rc-=pi[i]*column(j,i);
        if(!phase_one && rc>max_rc){max_rc=rc;max_rc_j=j;}
        const Real weight=options_.use_devex
            ? std::max<Real>(1.0,devex_weight[j]) : 1.0;
        const Real score=rc/std::sqrt(weight);
        if(score>best){best=score;enter=j;}
      }
      if(!phase_one && phase_iterations==0 && std::getenv("PHASE2_DEBUG")){
        std::cerr<<"[PHASE2_DEBUG] rows="<<M
                 <<" cols="<<total
                 <<" artificial_basic="<<artificial_basic_count
                 <<" nonzero_cost="<<nonzero_cost_count
                 <<" max_reduced_cost="<<max_rc
                 <<" max_rc_col="<<max_rc_j
                 <<" selected="<<enter
                 <<" basis_first="<<(M?sys.basis[0]:Index(-1))
                 <<" x_selected="<<((enter>=0)?x[enter]:0)<<"\\n";
      }
      if(!phase_one && std::getenv("PHASE2_DEBUG") && phase_iterations<10){
        std::cerr<<"[PHASE2_PIVOT] iter="<<phase_iterations
                 <<" enter="<<enter
                 <<" reduced_cost="<<((enter>=0)?max_rc:0)
                 <<" selected_score="<<best
                 <<" x_enter="<<((enter>=0)?x[enter]:0)<<"\\n";
      }
      if(enter<0){
        if(!phase_one && std::getenv("PHASE2_DEBUG")){
          std::cerr<<"[PHASE2_FINAL] iter="<<phase_iterations
                   <<" max_reduced_cost="<<max_rc
                   <<" max_rc_col="<<max_rc_j<<"\\n";
        }
        return SolveStatus::Optimal;
      }

      std::vector<Real> col(M,0);
      for(Index i=0;i<M;++i) col[i]=column(enter,i);
      if(!lu.solve(col,direction)) return SolveStatus::NumericalFailure;

      Real theta=std::numeric_limits<Real>::infinity();
      for(Index i=0;i<M;++i) if(direction[i]>options_.pivot_tolerance){
        const Real t=x[sys.basis[i]]/direction[i];
        if(t>=-options_.primal_tolerance)
          theta=std::min(theta,std::max<Real>(0,t));
      }
      if(!std::isfinite(theta)) return SolveStatus::Unbounded;

      const Real harris_tol=options_.primal_tolerance*
          std::max<Real>(1.0,std::abs(theta));
      const Real harris_upper=theta+harris_tol;
      Index leave=-1; Real best_pivot=-1;
      for(Index i=0;i<M;++i) if(direction[i]>options_.pivot_tolerance){
        const Real t=std::max<Real>(0,x[sys.basis[i]]/direction[i]);
        if(t<=harris_upper){
          const Real pivot=direction[i];
          if(pivot>best_pivot){best_pivot=pivot;leave=i;}
        }
      }
      if(leave<0){
        if(!phase_one && std::getenv("PHASE2_DEBUG"))
          std::cerr<<"[PHASE2_PIVOT] enter="<<enter<<" has no leaving row; direction is unbounded\\n";
        return SolveStatus::NumericalFailure;
      }

      if(!phase_one && std::getenv("PHASE2_DEBUG") && phase_iterations<10){
        std::cerr<<"[PHASE2_PIVOT] leave_row="<<leave
                 <<" leave_var="<<sys.basis[leave]
                 <<" pivot="<<direction[leave]
                 <<" theta="<<std::max<Real>(0,x[sys.basis[leave]]/direction[leave])<<"\\n";
      }

      theta=std::max<Real>(0,x[sys.basis[leave]]/direction[leave]);
      if(!phase_one && std::getenv("PHASE2_DEBUG") && theta>options_.primal_tolerance){
        std::cerr<<"[PHASE2_MOVE] iter="<<phase_iterations
                 <<" enter="<<enter
                 <<" leave="<<sys.basis[leave]
                 <<" theta="<<theta
                 <<" reduced_cost="<<max_rc<<"\\n";
      }
      for(Index i=0;i<M;++i) if(i!=leave)
        x[sys.basis[i]]-=theta*direction[i];

      if(options_.use_devex){
        Real new_weight=1.0;
        for(Index i=0;i<M;++i){
          const Real w=std::max<Real>(1.0,devex_weight[sys.basis[i]]);
          new_weight+=w*direction[i]*direction[i];
        }
        devex_weight[enter]=std::max<Real>(1.0,new_weight);
        if(devex_weight[enter]>1e12)
          for(Real& w:devex_weight) w=1.0;
      }

      x[enter]=theta;
      x[sys.basis[leave]]=0;
      sys.basis[leave]=enter;
      if(!refactor()) return SolveStatus::NumericalFailure;
    }
    return SolveStatus::IterationLimit;
  };

  // Phase I minimizes the sum of artificial variables. The internal simplex
  // convention maximizes c^T z, hence artificial costs are -1.
  std::vector<Real> phase1_c(total,0);
  for(Index j=0;j<total;++j) if(sys.artificial[j]) phase1_c[j]=-1;
  std::size_t phase1_iters=0;
  const SolveStatus p1=simplex_phase(phase1_c,true,phase1_iters);
  if(p1==SolveStatus::IterationLimit)
    return {p1,0,{}, {},0,0,iterations,
            "Phase I iteration limit reached."};
  if(p1!=SolveStatus::Optimal)
    return {p1,0,{}, {},0,0,iterations,
            "Phase I simplex failed."};

  Real artificial_sum=0;
  for(Index j=0;j<total;++j) if(sys.artificial[j])
    artificial_sum+=std::max<Real>(0,x[j]);
  if(artificial_sum>options_.primal_tolerance*std::max<Real>(1.0,M)){
    return {SolveStatus::Infeasible,0,{}, {},artificial_sum,0,iterations,
            "Phase I optimum is positive; model is infeasible."};
  }

  // Remove zero-valued artificial variables from the basis where possible.
  // A remaining zero artificial basic variable is harmless only when its row
  // is redundant, so it is retained but forbidden from entering Phase II.
  for(Index row=0;row<M;++row){
    const Index basic=sys.basis[row];
    if(!sys.artificial[basic] || x[basic]>options_.primal_tolerance) continue;

    std::vector<Real> cb(M,0), d;
    for(Index i=0;i<M;++i) cb[i]=0;
    if(!lu.solve_transpose(cb,pi)) return {SolveStatus::NumericalFailure,0,{}, {},0,0,iterations,"Phase I cleanup failed."};

    Index enter=-1;
    for(Index j=0;j<total;++j){
      if(sys.artificial[j]) continue;
      bool is_basic=false;
      for(Index q:sys.basis) if(q==j){is_basic=true;break;}
      if(is_basic) continue;
      std::vector<Real> col(M,0);
      for(Index i=0;i<M;++i) col[i]=column(j,i);
      if(!lu.solve(col,d)) return {SolveStatus::NumericalFailure,0,{}, {},0,0,iterations,"Phase I cleanup FTRAN failed."};
      if(std::abs(d[row])>options_.pivot_tolerance){ enter=j; break; }
    }
    if(enter>=0){
      x[enter]=0;
      x[basic]=0;
      sys.basis[row]=enter;
      if(!refactor()) return {SolveStatus::NumericalFailure,0,{}, {},0,0,iterations,"Phase I cleanup refactorization failed."};
    }
  }

  std::vector<Real> phase2_c(total,0);
  for(Index j=0;j<n;++j)
    phase2_c[j]=model.minimize ? -model.variables[j].objective
                               : model.variables[j].objective;

  std::size_t phase2_iters=0;
  const SolveStatus p2=simplex_phase(phase2_c,false,phase2_iters);
  if(p2==SolveStatus::IterationLimit)
    return {p2,0,{}, {},0,0,iterations,"Phase II iteration limit reached."};
  if(p2!=SolveStatus::Optimal)
    return {p2,0,{}, {},0,0,iterations,
            p2==SolveStatus::Unbounded?"Phase II found an unbounded objective.":
            "Phase II simplex failed."};

  std::vector<Real> primal(n,0);
  for(Index j=0;j<n;++j) primal[j]=shift[j]+std::max<Real>(0,x[j]);

  Real pres=0;
  std::vector<Real> ax;
  model.A.multiply(primal,ax);
  for(Index i=0;i<m;++i){
    pres=std::max(pres,std::max<Real>(0,model.constraints[i].lower_bound-ax[i]));
    pres=std::max(pres,std::max<Real>(0,ax[i]-model.constraints[i].upper_bound));
  }

  const Real objective=model.objective_value(primal);
  if(std::getenv("PHASE2_DEBUG")){
    Index positive_original=0;
    Real max_original=0;
    for(Index j=0;j<n;++j){
      if(primal[j]>options_.primal_tolerance) ++positive_original;
      max_original=std::max(max_original,std::abs(primal[j]));
    }
    std::cerr<<"[PHASE2_SOLUTION] objective="<<objective
             <<" positive_original="<<positive_original
             <<" max_abs_original="<<max_original
             <<" residual="<<pres<<"\\n";
  }
  return {SolveStatus::Optimal,objective,primal,{},pres,0,iterations,
          "Phase I feasible basis constructed; Phase II revised simplex optimal solution found."};
}
}
