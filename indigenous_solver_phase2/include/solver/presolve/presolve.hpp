#pragma once
#include <vector>
#include "solver/model/linear_model.hpp"
namespace solver {
struct PresolveStats { std::size_t fixed_variables=0, empty_rows=0, empty_columns=0, tightened_bounds=0; };
PresolveStats presolve(LinearModel& model);
}
