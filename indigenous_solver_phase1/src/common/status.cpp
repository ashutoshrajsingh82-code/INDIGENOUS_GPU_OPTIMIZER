#include "solver/common/status.hpp"
namespace solver {
std::string to_string(SolveStatus s) {
 switch(s){case SolveStatus::Optimal:return "OPTIMAL";case SolveStatus::Infeasible:return "INFEASIBLE";case SolveStatus::Unbounded:return "UNBOUNDED";case SolveStatus::IterationLimit:return "ITERATION_LIMIT";case SolveStatus::NumericalFailure:return "NUMERICAL_FAILURE";case SolveStatus::InvalidModel:return "INVALID_MODEL";case SolveStatus::UnsupportedModel:return "UNSUPPORTED_MODEL";default:return "ERROR";}
}
}
