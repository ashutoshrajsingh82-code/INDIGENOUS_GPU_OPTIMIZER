#pragma once
#include <vector>
#include "solver/common/types.hpp"
namespace solver {
class CsrMatrix;
class CscMatrix {
 public:
  CscMatrix()=default; CscMatrix(Index r, Index c, std::vector<Real> v, std::vector<Index> ri, std::vector<Index> cp);
  Index rows() const{return rows_;} Index cols() const{return cols_;}
  const std::vector<Real>& values() const{return values_;} const std::vector<Index>& row_indices() const{return row_indices_;} const std::vector<Index>& column_pointers() const{return column_pointers_;}
  void multiply(const std::vector<Real>& x,std::vector<Real>& y) const;
  void transpose_multiply(const std::vector<Real>& x,std::vector<Real>& y) const;
  CsrMatrix to_csr() const;
 private: Index rows_=0,cols_=0; std::vector<Real> values_; std::vector<Index> row_indices_,column_pointers_;
};
}
