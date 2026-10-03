#pragma once
#include "solver/common/types.hpp"
namespace solver { struct Tolerances { Real primal_feasibility=1e-8; Real dual_feasibility=1e-8; Real optimality=1e-8; Real zero=1e-12; }; }
