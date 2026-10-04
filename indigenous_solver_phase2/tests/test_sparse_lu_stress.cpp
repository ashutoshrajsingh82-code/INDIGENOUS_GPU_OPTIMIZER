#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
#include <random>
#include <vector>

#include "solver/linalg/sparse_lu.hpp"

using namespace solver;

static int fails=0;

static void check(bool ok, const char* message) {
  if(!ok) {
    std::cerr << "FAIL: " << message << "\n";
    ++fails;
  }
}

static Real max_residual(const std::vector<std::vector<Real>>& a,
                         const std::vector<Real>& x,
                         const std::vector<Real>& b) {
  Real residual=0;
  for(Index i=0;i<a.size();++i) {
    Real ax=0;
    for(Index j=0;j<a.size();++j) ax+=a[i][j]*x[j];
    residual=std::max(residual,std::abs(ax-b[i]));
  }
  return residual;
}

static Real max_transpose_residual(const std::vector<std::vector<Real>>& a,
                                   const std::vector<Real>& x,
                                   const std::vector<Real>& b) {
  Real residual=0;
  for(Index j=0;j<a.size();++j) {
    Real atx=0;
    for(Index i=0;i<a.size();++i) atx+=a[i][j]*x[i];
    residual=std::max(residual,std::abs(atx-b[j]));
  }
  return residual;
}

static std::vector<Real> multiply(const std::vector<std::vector<Real>>& a,
                                  const std::vector<Real>& x) {
  std::vector<Real> b(a.size(),0);
  for(Index i=0;i<a.size();++i)
    for(Index j=0;j<a.size();++j)
      b[i]+=a[i][j]*x[j];
  return b;
}

int main() {
  // Fixed matrix requiring a row interchange at the first pivot.
  {
    SparseLU lu;
    const std::vector<std::vector<Real>> a{
      {0,2,1},
      {3,1,2},
      {1,4,5}
    };
    const std::vector<Real> expected{1.25,-0.5,2.0};
    const auto b=multiply(a,expected);
    std::vector<Real> x,xt;

    check(lu.factorize(a),"fixed pivoting factorization");
    check(lu.solve(b,x),"fixed pivoting solve");
    check(max_residual(a,x,b)<1e-10,"fixed pivoting residual");
    check(lu.solve_transpose(b,xt),"fixed transpose solve");
    check(max_transpose_residual(a,xt,b)<1e-10,"fixed transpose residual");
  }

  // Deterministic random diagonally-dominant matrices. These are safely
  // nonsingular while still exercising different pivot patterns and sparsity.
  std::mt19937 rng(26119);
  std::uniform_real_distribution<Real> offdiag(-1.5,1.5);
  std::uniform_real_distribution<Real> xdist(-2.0,2.0);

  int cases=0;
  for(Index n=2;n<=10;++n) {
    for(int case_id=0;case_id<20;++case_id) {
      std::vector<std::vector<Real>> a(n,std::vector<Real>(n,0));

      for(Index i=0;i<n;++i) {
        Real row_sum=0;
        for(Index j=0;j<n;++j) {
          if(i==j) continue;
          // Keep about 35% of off-diagonal entries nonzero.
          if((rng()%20)<7) {
            a[i][j]=offdiag(rng);
            row_sum+=std::abs(a[i][j]);
          }
        }
        a[i][i]=row_sum+1.0;
      }

      // Apply a deterministic row permutation. This forces the
      // factorization to cope with reordered rows without changing
      // nonsingularity.
      std::vector<Index> permutation(n);
      std::iota(permutation.begin(),permutation.end(),0);
      std::shuffle(permutation.begin(),permutation.end(),rng);

      std::vector<std::vector<Real>> permuted(n,std::vector<Real>(n,0));
      for(Index i=0;i<n;++i) permuted[i]=a[permutation[i]];

      std::vector<Real> expected(n);
      for(Real& value:expected) value=xdist(rng);
      const auto b=multiply(permuted,expected);

      SparseLU lu;
      std::vector<Real> x,xt;
      check(lu.factorize(permuted),"random factorization");
      check(lu.solve(b,x),"random solve");
      check(max_residual(permuted,x,b)<1e-9,"random solve residual");
      check(lu.solve_transpose(b,xt),"random transpose solve");
      check(max_transpose_residual(permuted,xt,b)<1e-9,
            "random transpose residual");

      ++cases;
    }
  }

  std::cout << "SparseLU stress cases: " << cases << "\n";
  std::cout << (fails ? "SPARSE LU STRESS TESTS FAILED"
                       : "SPARSE LU STRESS TESTS PASSED") << "\n";
  return fails ? 1 : 0;
}
