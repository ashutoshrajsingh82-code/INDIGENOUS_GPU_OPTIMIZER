#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <string>
#include <cstdlib>
#include <optional>
#include <iostream>
#include <vector>
#include "solver/simplex/revised_simplex.hpp"
#include "solver/linalg/sparse_lu.hpp"
#include "solver/presolve/presolve.hpp"
#include "gpu/sparse_pricing.hpp"

namespace solver {
namespace {
constexpr Real kEqTol=1e-10;
constexpr std::size_t kMaxEtaUpdates=32;
static bool phase2_debug_enabled() {
#ifdef _WIN32
  char* value=nullptr;
  std::size_t size=0;
  if(_dupenv_s(&value,&size,"PHASE2_DEBUG")!=0 || value==nullptr) return false;
  const bool enabled=value[0] && value[0]!='0';
  std::free(value);
  return enabled;
#else
  const char* value=std::getenv("PHASE2_DEBUG");
  return value && value[0] && value[0]!='0';
#endif
}

const bool kPhase2Debug = phase2_debug_enabled();

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
  const auto solve_start=std::chrono::steady_clock::now();
  SimplexStatistics stats;
  auto elapsed_ms=[](auto start){
    return std::chrono::duration<double,std::milli>(
      std::chrono::steady_clock::now()-start).count();
  };
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

  // Cache sparse standard-form columns once. Refactorization can then
  // consume only the nonzeros of the current basis instead of materializing
  // a dense M-by-M basis matrix on every eta-chain reset.
  std::vector<std::vector<std::pair<Index,Real>>> sparse_columns(total);
  for(Index i=0;i<M;++i)
    for(Index j=0;j<total;++j)
      if(sys.A[i][j]!=0) sparse_columns[j].push_back({i,sys.A[i][j]});

  SparseLU lu;
  auto column=[&](Index j, Index i)->Real { return sys.A[i][j]; };
  auto refactor=[&](){
    const auto start=std::chrono::steady_clock::now();
    std::vector<std::vector<std::pair<Index,Real>>> basis_columns(M);
    for(Index k=0;k<M;++k)
      basis_columns[k]=sparse_columns[sys.basis[k]];
    const bool ok=lu.factorize_sparse_columns(
        basis_columns,M,options_.pivot_tolerance);
    stats.lu_factorization_ms+=elapsed_ms(start);
    stats.lu_factor_load_ms+=lu.last_factor_load_ms();
    stats.lu_factor_pivot_ms+=lu.last_factor_pivot_ms();
    stats.lu_factor_elimination_ms+=lu.last_factor_elimination_ms();
    stats.lu_factor_elimination_affected_rows+=lu.last_factor_elimination_affected_rows();
    stats.lu_factor_elimination_row_scan_checks+=lu.last_factor_elimination_row_scan_checks();
    stats.lu_factor_elimination_pivot_entries+=lu.last_factor_elimination_pivot_entries();
    stats.lu_factor_elimination_hash_finds+=lu.last_factor_elimination_hash_finds();
    stats.lu_factor_elimination_hash_inserts+=lu.last_factor_elimination_hash_inserts();
    stats.lu_factor_elimination_hash_erases+=lu.last_factor_elimination_hash_erases();
    ++stats.lu_factorizations;
    stats.max_lu_nonzeros=std::max(
      stats.max_lu_nonzeros,lu.l_nonzeros()+lu.u_nonzeros());
    return ok;
  };

  if(!refactor())
    return {SolveStatus::NumericalFailure,0,{}, {},0,0,0,
            "Initial Phase I basis factorization failed."};

  std::vector<Real> x(total,0);
  for(Index i=0;i<M;++i) x[sys.basis[i]]=sys.b[i];
  std::vector<bool> is_basic(total,false);
  for(Index q:sys.basis) is_basic[q]=true;
  std::vector<Real> devex_weight(total,1.0);
  std::vector<Real> pi, direction;
  std::size_t iterations=0;

  // Phase 3 pricing consumes CSC data. The standard-form matrix is immutable
  // throughout simplex, so build this representation once and reuse it for
  // every pricing iteration instead of rebuilding it inside the hot loop.
  std::vector<std::size_t> pricing_offsets(total+1,0);
  std::vector<std::size_t> pricing_rows;
  std::vector<double> pricing_values;
  std::size_t pricing_nnz=0;
  for(const auto& column_entries:sparse_columns) pricing_nnz+=column_entries.size();
  pricing_rows.reserve(pricing_nnz);
  pricing_values.reserve(pricing_nnz);
  for(Index j=0;j<total;++j){
    pricing_offsets[static_cast<std::size_t>(j)]=pricing_rows.size();
    for(const auto& [i,value]:sparse_columns[j]){
      pricing_rows.push_back(static_cast<std::size_t>(i));
      pricing_values.push_back(value);
    }
  }
  pricing_offsets[static_cast<std::size_t>(total)]=pricing_rows.size();

  auto simplex_phase = [&](const std::vector<Real>& c, bool phase_one,
                           std::size_t& phase_iterations)->SolveStatus {
    phase_iterations=0;
    for(;phase_iterations<options_.max_iterations && iterations<options_.max_iterations;
        ++phase_iterations,++iterations){
      std::vector<Real> cb(M,0);
      for(Index i=0;i<M;++i) cb[i]=c[sys.basis[i]];
      {
        const auto start=std::chrono::steady_clock::now();
        if(!lu.solve_transpose(cb,pi)) return SolveStatus::NumericalFailure;
        stats.btran_ms+=elapsed_ms(start);
        stats.eta_transpose_ms+=lu.last_eta_transpose_ms();
        ++stats.btran_solves;
      }
      if(!phase_one && kPhase2Debug && phase_iterations<3){
        Real dual_res=0;
        for(Index k=0;k<M;++k){
          Real lhs=0;
          for(Index i=0;i<M;++i) lhs+=column(sys.basis[k],i)*pi[i];
          dual_res=std::max(dual_res,std::abs(lhs-cb[k]));
        }
        std::cerr<<"[PHASE2_LU] iter="<<phase_iterations
                 <<" dual_residual="<<dual_res<<"\\n";
      }

      const auto pricing_start=std::chrono::steady_clock::now();
      Index enter=-1; Real best=options_.dual_tolerance;
      Real max_rc=-std::numeric_limits<Real>::infinity();
      Index max_rc_j=-1;
      std::size_t artificial_basic_count=0, nonzero_cost_count=0;
      for(Index q:sys.basis) if(sys.artificial[q]) ++artificial_basic_count;
      for(Index j=0;j<total;++j)
        if(std::abs(c[j])>options_.dual_tolerance) ++nonzero_cost_count;

      // Phase 3 pricing reuses the immutable CSC representation built
      // before the simplex iterations. On an NVIDIA build this dispatches to
      // CUDA; on this machine it uses the validated CPU fallback. The original
      // scalar loop remains available through PHASE3_DISABLE_GPU_PRICING.
      bool use_phase3_pricing=true;
#ifdef _WIN32
      char* disable_value=nullptr;
      std::size_t disable_size=0;
      if(_dupenv_s(&disable_value,&disable_size,"PHASE3_DISABLE_GPU_PRICING")==0 &&
         disable_value!=nullptr){
        use_phase3_pricing=!(disable_value[0] && disable_value[0]!='0');
        std::free(disable_value);
      }
#else
      const char* disable_value=std::getenv("PHASE3_DISABLE_GPU_PRICING");
      use_phase3_pricing=!(disable_value && disable_value[0] && disable_value[0]!='0');
#endif

      std::vector<Real> reduced_costs;
      bool backend_pricing_ok=false;
      if(use_phase3_pricing){
        backend_pricing_ok=indigenous::gpu::sparse_reduced_costs(
            pricing_offsets,pricing_rows,pricing_values,c,pi,reduced_costs);
      }

      if(backend_pricing_ok){
        for(Index j=0;j<total;++j){
          if(is_basic[j]) continue;
          if(!phase_one && sys.artificial[j]) continue;

          const Real rc=reduced_costs[static_cast<std::size_t>(j)];
          if(!phase_one && rc>max_rc){max_rc=rc;max_rc_j=j;}
          const Real weight=options_.use_devex
              ? std::max<Real>(1.0,devex_weight[j]) : 1.0;
          const Real score=rc/std::sqrt(weight);
          if(score>best){best=score;enter=j;}
        }
      } else {
        // Preserve the original CPU pricing path as a correctness and
        // failure fallback if the backend rejects the sparse representation.
        for(Index j=0;j<total;++j){
          if(is_basic[j]) continue;
          if(!phase_one && sys.artificial[j]) continue;

          Real rc=c[j];
          for(const auto& [i,value] : sparse_columns[j]) rc-=pi[i]*value;
          if(!phase_one && rc>max_rc){max_rc=rc;max_rc_j=j;}
          const Real weight=options_.use_devex
              ? std::max<Real>(1.0,devex_weight[j]) : 1.0;
          const Real score=rc/std::sqrt(weight);
          if(score>best){best=score;enter=j;}
        }
      }
      stats.pricing_ms+=elapsed_ms(pricing_start);
      if(!phase_one && phase_iterations==0 && kPhase2Debug){
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
      if(!phase_one && kPhase2Debug && phase_iterations<10){
        std::cerr<<"[PHASE2_PIVOT] iter="<<phase_iterations
                 <<" enter="<<enter
                 <<" reduced_cost="<<((enter>=0)?max_rc:0)
                 <<" selected_score="<<best
                 <<" x_enter="<<((enter>=0)?x[enter]:0)<<"\\n";
      }
      if(enter<0){
        if(!phase_one && kPhase2Debug){
          std::cerr<<"[PHASE2_FINAL] iter="<<phase_iterations
                   <<" max_reduced_cost="<<max_rc
                   <<" max_rc_col="<<max_rc_j<<"\\n";
        }
        return SolveStatus::Optimal;
      }

      std::vector<Real> col(M,0);
      for(const auto& [i,value] : sparse_columns[enter]) col[i]=value;
      {
        const auto start=std::chrono::steady_clock::now();
        if(!lu.solve(col,direction)) return SolveStatus::NumericalFailure;
        stats.ftran_ms+=elapsed_ms(start);
        stats.eta_forward_ms+=lu.last_eta_forward_ms();
        ++stats.ftran_solves;
      }
      if(!phase_one && kPhase2Debug && phase_iterations<3){
        Real ftran_res=0;
        for(Index i=0;i<M;++i){
          Real lhs=0;
          for(Index k=0;k<M;++k) lhs+=column(sys.basis[k],i)*direction[k];
          ftran_res=std::max(ftran_res,std::abs(lhs-col[i]));
        }
        std::cerr<<"[PHASE2_LU] iter="<<phase_iterations
                 <<" ftran_residual="<<ftran_res<<"\\n";
      }

      const auto ratio_start=std::chrono::steady_clock::now();
      Real theta=std::numeric_limits<Real>::infinity();
      for(Index i=0;i<M;++i) if(direction[i]>options_.pivot_tolerance){
        const Real t=x[sys.basis[i]]/direction[i];
        if(t>=-options_.primal_tolerance)
          theta=std::min(theta,std::max<Real>(0,t));
      }
      if(!std::isfinite(theta)) return SolveStatus::Unbounded;

      stats.ratio_test_ms+=elapsed_ms(ratio_start);
      const auto basis_update_start=std::chrono::steady_clock::now();
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
        if(!phase_one && kPhase2Debug)
          std::cerr<<"[PHASE2_PIVOT] enter="<<enter<<" has no leaving row; direction is unbounded\\n";
        return SolveStatus::NumericalFailure;
      }

      if(!phase_one && kPhase2Debug && phase_iterations<10){
        std::cerr<<"[PHASE2_PIVOT] leave_row="<<leave
                 <<" leave_var="<<sys.basis[leave]
                 <<" pivot="<<direction[leave]
                 <<" theta="<<std::max<Real>(0,x[sys.basis[leave]]/direction[leave])<<"\\n";
      }

      theta=std::max<Real>(0,x[sys.basis[leave]]/direction[leave]);

      // Degenerate pivots (theta == 0) are common in Netlib models. Prefer
      // the smallest basic index on a degenerate ratio tie instead of the
      // largest pivot; this deterministic Bland-style tie-break prevents
      // short cycling on highly degenerate bases such as bore3d.
      if(theta<=options_.primal_tolerance){
        Index bland_leave=-1;
        for(Index i=0;i<M;++i) if(direction[i]>options_.pivot_tolerance){
          const Real t=std::max<Real>(0,x[sys.basis[i]]/direction[i]);
          if(t<=harris_upper &&
             (bland_leave<0 || sys.basis[i]<sys.basis[bland_leave]))
            bland_leave=i;
        }
        if(bland_leave>=0) leave=bland_leave;
        theta=std::max<Real>(0,x[sys.basis[leave]]/direction[leave]);
      }

      if(!phase_one && kPhase2Debug && theta>options_.primal_tolerance){
        std::cerr<<"[PHASE2_MOVE] iter="<<phase_iterations
                 <<" enter="<<enter
                 <<" leave="<<sys.basis[leave]
                 <<" theta="<<theta
                 <<" reduced_cost="<<max_rc<<"\\n";
      }
      // Update the primal basic values and, when enabled, the entering
      // variable's Devex weight in one row pass. Both operations consume
      // the same basis/direction data, so combining them avoids a second
      // full scan of the basis on every pivot.
      Real new_weight=1.0;
      for(Index i=0;i<M;++i){
        const Index basic=sys.basis[i];
        if(i!=leave) x[basic]-=theta*direction[i];
        if(options_.use_devex){
          const Real w=std::max<Real>(1.0,devex_weight[basic]);
          new_weight+=w*direction[i]*direction[i];
        }
      }

      if(options_.use_devex){
        devex_weight[enter]=std::max<Real>(1.0,new_weight);
        if(devex_weight[enter]>1e12)
          for(Real& w:devex_weight) w=1.0;
      }

      x[enter]=theta;
      x[sys.basis[leave]]=0;
      is_basic[sys.basis[leave]]=false;
      sys.basis[leave]=enter;
      is_basic[enter]=true;
      ++stats.pivots;

      // Keep the existing LU factors and represent this column replacement
      // as a product-form eta update. Full refactorization is deferred until
      // the eta chain reaches a bounded length, preventing one expensive
      // sparse factorization per simplex pivot.
      const auto lu_update_start=std::chrono::steady_clock::now();
      if(!lu.update(direction,leave,options_.pivot_tolerance))
        return SolveStatus::NumericalFailure;
      stats.lu_update_ms+=elapsed_ms(lu_update_start);
      ++stats.lu_updates;

      if(lu.update_count()>=kMaxEtaUpdates){
        if(!refactor()) return SolveStatus::NumericalFailure;
      }
      stats.basis_update_ms+=elapsed_ms(basis_update_start);
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

  if(kPhase2Debug){
    Real max_rhs=0, max_basic=0;
    Index positive_basic=0;
    for(Index i=0;i<M;++i){
      max_rhs=std::max(max_rhs,std::abs(sys.b[i]));
      const Real xb=x[sys.basis[i]];
      max_basic=std::max(max_basic,std::abs(xb));
      if(xb>options_.primal_tolerance) ++positive_basic;
    }
    std::cerr<<"[PHASE2_P1] iterations="<<phase1_iters
             <<" artificial_sum="<<artificial_sum
             <<" max_rhs="<<max_rhs
             <<" max_basic="<<max_basic
             <<" positive_basic="<<positive_basic<<"\\n";
  }
  if(artificial_sum>options_.primal_tolerance*std::max<Real>(1.0,static_cast<Real>(M))){
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
      if(is_basic[j]) continue;
      std::vector<Real> col(M,0);
      for(const auto& [i,value] : sparse_columns[j]) col[i]=value;
      if(!lu.solve(col,d)) return {SolveStatus::NumericalFailure,0,{}, {},0,0,iterations,"Phase I cleanup FTRAN failed."};
      if(std::abs(d[row])>options_.pivot_tolerance){ enter=j; break; }
    }
    if(enter>=0){
      x[enter]=0;
      x[basic]=0;
      is_basic[basic]=false;
      sys.basis[row]=enter;
      is_basic[enter]=true;
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

  // Refine the final basic solution against the current LU factors.
  // Sparse dropping and long degenerate sequences can leave a small basis
  // residual even after the reduced-cost test is optimal. Iterative refinement
  // improves the returned primal certificate without changing the basis.
  for(int refinement=0; refinement<3; ++refinement){
    std::vector<Real> residual(M,0);
    Real max_residual=0, max_rhs=0;
    for(Index i=0;i<M;++i){
      Real lhs=0;
      for(Index j=0;j<total;++j) lhs+=sys.A[i][j]*x[j];
      residual[i]=lhs-sys.b[i];
      max_residual=std::max(max_residual,std::abs(residual[i]));
      max_rhs=std::max(max_rhs,std::abs(sys.b[i]));
    }
    if(max_residual<=1e-11*std::max<Real>(1.0,max_rhs)) break;

    std::vector<Real> correction_rhs(M,0), correction;
    for(Index i=0;i<M;++i) correction_rhs[i]=-residual[i];
    if(!lu.solve(correction_rhs,correction)) break;

    std::vector<Real> candidate=x;
    bool nonnegative=true;
    for(Index i=0;i<M;++i){
      const Index basic=sys.basis[i];
      candidate[static_cast<std::size_t>(basic)]+=correction[i];
      if(candidate[basic] < -options_.primal_tolerance){
        nonnegative=false;
        break;
      }
    }
    if(!nonnegative) break;
    for(Index i=0;i<M;++i)
      if(candidate[sys.basis[i]]<0) candidate[sys.basis[i]]=0;
    x.swap(candidate);
  }

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
  if(kPhase2Debug){
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
  stats.pivot_ms=stats.ratio_test_ms+stats.basis_update_ms;
  stats.total_ms=elapsed_ms(solve_start);
  return {SolveStatus::Optimal,objective,primal,{},pres,0,iterations,
          "Phase I feasible basis constructed; Phase II revised simplex optimal solution found.",
          stats};
}
}
