#pragma once
#include <string>
namespace solver {
enum class SolveStatus { Optimal, Infeasible, Unbounded, IterationLimit, NumericalFailure, InvalidModel, UnsupportedModel, Error };
std::string to_string(SolveStatus s);
}
