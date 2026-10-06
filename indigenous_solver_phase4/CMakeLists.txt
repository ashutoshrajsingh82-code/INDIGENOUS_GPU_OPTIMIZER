cmake_minimum_required(VERSION 3.20)
project(IndigenousSolverPhase4 VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(indigenous_phase4_basis
  src/basis/cpu_basis_solver.cpp
  src/basis/basis_validation.cpp
  ../indigenous_solver_phase2/src/linalg/sparse_lu.cpp
)

target_include_directories(indigenous_phase4_basis
  PUBLIC
    include
    ../indigenous_solver_phase1/include
    ../indigenous_solver_phase2/include
)

add_executable(indigenous_phase4_basis_test
  tests/basis_solver_test.cpp
)
target_link_libraries(indigenous_phase4_basis_test PRIVATE indigenous_phase4_basis)

add_executable(indigenous_phase4_ftran_validation_test
  tests/ftran_validation_test.cpp
)
target_link_libraries(indigenous_phase4_ftran_validation_test PRIVATE indigenous_phase4_basis)

add_executable(indigenous_phase4_btran_validation_test
  tests/btran_validation_test.cpp
)
target_link_libraries(indigenous_phase4_btran_validation_test PRIVATE indigenous_phase4_basis)

enable_testing()
add_test(NAME indigenous_phase4_basis_solver
         COMMAND indigenous_phase4_basis_test)
add_test(NAME indigenous_phase4_ftran_validation
         COMMAND indigenous_phase4_ftran_validation_test)
add_test(NAME indigenous_phase4_btran_validation
         COMMAND indigenous_phase4_btran_validation_test)
