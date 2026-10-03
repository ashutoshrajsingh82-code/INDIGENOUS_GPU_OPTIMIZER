#pragma once
#include <vector>
#include "solver/common/types.hpp"
namespace solver {
class CscMatrix;
class CsrMatrix {
 public:
  CsrMatrix()=default; CsrMatrix(Index r, Index c, std::vector<Real> v, std::vector<Index> ci, std::vector<Index> rp);
  Index rows() const{return rows_;} Index cols() const{return cols_;}
  const std::vector<Real>& values() const{return values_;} const std::vector<Index>& column_indices() const{return column_indices_;} const std::vector<Index>& row_pointers() const{return row_pointers_;}
  void multiply(const std::vector<Real>& x,std::vector<Real>& y) const;
  CscMatrix to_csc() const;
 private: Index rows_=0,cols_=0; std::vector<Real> values_; std::vector<Index> column_indices_,row_pointers_;
};
}
