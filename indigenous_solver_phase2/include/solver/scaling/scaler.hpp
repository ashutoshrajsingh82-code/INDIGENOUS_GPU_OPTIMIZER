#pragma once
#include <vector>
#include "solver/model/linear_model.hpp"
namespace solver {
struct ScalingResult { std::vector<Real> row_scale, col_scale; Real max_abs=0; };
ScalingResult scale_model(LinearModel& model);
}
