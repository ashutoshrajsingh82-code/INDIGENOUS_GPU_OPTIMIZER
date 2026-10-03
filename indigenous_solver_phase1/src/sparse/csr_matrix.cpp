#include "solver/sparse/csr_matrix.hpp"
#include "solver/sparse/csc_matrix.hpp"
#include <stdexcept>
namespace solver {
CsrMatrix::CsrMatrix(Index r,Index c,std::vector<Real> v,std::vector<Index> ci,std::vector<Index> rp):rows_(r),cols_(c),values_(std::move(v)),column_indices_(std::move(ci)),row_pointers_(std::move(rp)){if((Index)row_pointers_.size()!=rows_+1||(Index)values_.size()!=(Index)column_indices_.size())throw std::invalid_argument("invalid CSR arrays");}
void CsrMatrix::multiply(const std::vector<Real>& x,std::vector<Real>& y) const {if((Index)x.size()!=cols_)throw std::invalid_argument("CSR multiply dimension mismatch");y.assign(rows_,0);for(Index i=0;i<rows_;++i)for(Index p=row_pointers_[i];p<row_pointers_[i+1];++p)y[i]+=values_[p]*x[column_indices_[p]];}
CscMatrix CsrMatrix::to_csc() const {std::vector<Index> cp(cols_+1,0);for(auto c:column_indices_)++cp[c+1];for(Index j=0;j<cols_;++j)cp[j+1]+=cp[j];std::vector<Real> v(values_.size());std::vector<Index> ri(values_.size());auto next=cp;for(Index i=0;i<rows_;++i)for(Index p=row_pointers_[i];p<row_pointers_[i+1];++p){auto c=column_indices_[p];auto q=next[c]++;v[q]=values_[p];ri[q]=i;}return CscMatrix(rows_,cols_,std::move(v),std::move(ri),std::move(cp));}
}
