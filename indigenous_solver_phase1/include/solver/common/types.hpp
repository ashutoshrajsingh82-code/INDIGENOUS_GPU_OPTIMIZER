#pragma once
#include <cstdint>
#include <limits>
namespace solver {
using Real = double;
using Index = std::int64_t;
constexpr Real kInfinity = std::numeric_limits<Real>::infinity();
}
