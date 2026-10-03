#include "solver/model/linear_model.hpp"
#include <cmath>
namespace solver {
bool LinearModel::validate(std::string& e) const {if((Index)variables.size()!=A.cols()){e="variable count does not match matrix columns";return false;}if((Index)constraints.size()!=A.rows()){e="constraint count does not match matrix rows";return false;}for(const auto& v:variables){if(v.lower_bound>v.upper_bound){e="variable lower bound exceeds upper bound: "+v.name;return false;}if(!std::isfinite(v.objective)){e="non-finite objective coefficient: "+v.name;return false;}if(std::isnan(v.lower_bound)||std::isnan(v.upper_bound)){e="NaN variable bound: "+v.name;return false;}}for(const auto& c:constraints)if(c.lower_bound>c.upper_bound){e="constraint lower bound exceeds upper bound: "+c.name;return false;}return true;}
Real LinearModel::objective_value(const std::vector<Real>& x) const {Real z=0;for(std::size_t i=0;i<x.size()&&i<variables.size();++i)z+=variables[i].objective*x[i];return z;}
}
