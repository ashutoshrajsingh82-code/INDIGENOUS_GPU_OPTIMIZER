cmake_minimum_required(VERSION 3.20)
project(IndigenousSolverPhase2 VERSION 0.2.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(solver_phase2
  ../indigenous_solver_phase1/src/common/status.cpp
  ../indigenous_solver_phase1/src/model/linear_model.cpp
  ../indigenous_solver_phase1/src/model/solution_validator.cpp
  ../indigenous_solver_phase1/src/sparse/csc_matrix.cpp
  ../indigenous_solver_phase1/src/sparse/csr_matrix.cpp
  ../indigenous_solver_phase1/src/io/lp_reader.cpp
  ../indigenous_solver_phase1/src/io/mps_reader.cpp
  src/linalg/sparse_lu.cpp
  src/scaling/scaler.cpp
  src/presolve/presolve.cpp
  src/simplex/revised_simplex.cpp
)
target_include_directories(solver_phase2 PUBLIC include ../indigenous_solver_phase1/include)

add_executable(solver_phase2_cli tools/solver_cli.cpp)
target_link_libraries(solver_phase2_cli PRIVATE solver_phase2)

add_executable(solver_phase2_tests tests/test_phase2.cpp)
target_link_libraries(solver_phase2_tests PRIVATE solver_phase2)

add_executable(solver_phase2_random_lp_tests tests/test_random_lp.cpp)
target_link_libraries(solver_phase2_random_lp_tests PRIVATE solver_phase2)

add_executable(solver_phase2_sparse_lu_stress_tests tests/test_sparse_lu_stress.cpp)
target_link_libraries(solver_phase2_sparse_lu_stress_tests PRIVATE solver_phase2)

enable_testing()
add_test(NAME solver_phase2_tests COMMAND solver_phase2_tests)
add_test(NAME solver_phase2_random_lp_tests COMMAND solver_phase2_random_lp_tests)
add_test(NAME solver_phase2_sparse_lu_stress_tests COMMAND solver_phase2_sparse_lu_stress_tests)
