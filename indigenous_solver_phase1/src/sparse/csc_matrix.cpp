#include "solver/sparse/csc_matrix.hpp"
#include "solver/sparse/csr_matrix.hpp"
#include <stdexcept>
namespace solver {
CscMatrix::CscMatrix(Index r,Index c,std::vector<Real> v,std::vector<Index> ri,std::vector<Index> cp):rows_(r),cols_(c),values_(std::move(v)),row_indices_(std::move(ri)),column_pointers_(std::move(cp)){if((Index)column_pointers_.size()!=cols_+1||(Index)values_.size()!=(Index)row_indices_.size())throw std::invalid_argument("invalid CSC arrays");}
void CscMatrix::multiply(const std::vector<Real>& x,std::vector<Real>& y) const {if((Index)x.size()!=cols_)throw std::invalid_argument("CSC multiply dimension mismatch");y.assign(rows_,0);for(Index j=0;j<cols_;++j)for(Index p=column_pointers_[j];p<column_pointers_[j+1];++p)y[row_indices_[p]]+=values_[p]*x[j];}
void CscMatrix::transpose_multiply(const std::vector<Real>& x,std::vector<Real>& y) const {if((Index)x.size()!=rows_)throw std::invalid_argument("CSC transpose dimension mismatch");y.assign(cols_,0);for(Index j=0;j<cols_;++j)for(Index p=column_pointers_[j];p<column_pointers_[j+1];++p)y[j]+=values_[p]*x[row_indices_[p]];}
CsrMatrix CscMatrix::to_csr() const {std::vector<Index> rp(rows_+1,0);for(auto r:row_indices_)++rp[r+1];for(Index i=0;i<rows_;++i)rp[i+1]+=rp[i];std::vector<Real> v(values_.size());std::vector<Index> ci(values_.size());auto next=rp;for(Index j=0;j<cols_;++j)for(Index p=column_pointers_[j];p<column_pointers_[j+1];++p){auto r=row_indices_[p];auto q=next[r]++;v[q]=values_[p];ci[q]=j;}return CsrMatrix(rows_,cols_,std::move(v),std::move(ci),std::move(rp));}
}
