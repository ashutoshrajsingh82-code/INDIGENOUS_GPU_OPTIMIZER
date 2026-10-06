cmake_minimum_required(VERSION 3.20)
project(IndigenousSolverPhase4 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CheckLanguage)
check_language(CUDA)

if(CMAKE_CUDA_COMPILER)
  enable_language(CUDA)
  message(STATUS "Phase 4 backend: CUDA toolchain detected")
else()
  message(STATUS "Phase 4 backend: CUDA unavailable, using CPU basis solver")
endif()

add_library(indigenous_phase4_basis
  src/gpu/basis_solver_cpu.cpp
)

target_include_directories(indigenous_phase4_basis PUBLIC include)

add_executable(indigenous_phase4_basis_test
  tests/basis_solver_test.cpp
)
target_link_libraries(indigenous_phase4_basis_test PRIVATE indigenous_phase4_basis)

enable_testing()
add_test(NAME indigenous_phase4_basis_test
         COMMAND indigenous_phase4_basis_test)
