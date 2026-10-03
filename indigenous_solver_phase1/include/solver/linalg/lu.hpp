#pragma once
#include <vector>
#include "solver/common/types.hpp"
namespace solver {
class LUFactorization { public: bool factorize(const std::vector<std::vector<Real>>& A, Real tol=1e-12); bool solve(const std::vector<Real>& b,std::vector<Real>& x) const; Index size() const{return n_;} private: Index n_=0; std::vector<std::vector<Real>> lu_; std::vector<Index> piv_; Real tol_=1e-12; };
}
