cmake_minimum_required(VERSION 3.20)
project(IndigenousSolver VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(solver_core
  src/common/status.cpp
  src/model/linear_model.cpp
  src/model/solution_validator.cpp
  src/sparse/csc_matrix.cpp
  src/sparse/csr_matrix.cpp
  src/io/lp_reader.cpp
  src/io/mps_reader.cpp
  src/linalg/lu.cpp
  src/simplex/primal_simplex.cpp
  src/simplex/dual_simplex.cpp
)
target_include_directories(solver_core PUBLIC include)

add_executable(solver_cli tools/solver_cli.cpp)
target_link_libraries(solver_cli PRIVATE solver_core)

add_executable(solver_tests tests/test_main.cpp)
target_link_libraries(solver_tests PRIVATE solver_core)

enable_testing()
add_test(NAME solver_tests COMMAND solver_tests)
