#pragma once
#include <string>
#include "solver/model/linear_model.hpp"
namespace solver { bool read_lp(const std::string& path, LinearModel& model, std::string& error); }
