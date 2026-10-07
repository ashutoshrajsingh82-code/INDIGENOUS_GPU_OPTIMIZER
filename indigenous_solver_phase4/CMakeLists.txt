cmake_minimum_required(VERSION 3.20)
project(IndigenousSolverPhase4 VERSION 0.3.0 LANGUAGES CXX)

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

add_executable(indigenous_phase4_basis_test tests/basis_solver_test.cpp)
target_link_libraries(indigenous_phase4_basis_test PRIVATE indigenous_phase4_basis)

add_executable(indigenous_phase4_ftran_validation_test tests/ftran_validation_test.cpp)
target_link_libraries(indigenous_phase4_ftran_validation_test PRIVATE indigenous_phase4_basis)

add_executable(indigenous_phase4_btran_validation_test tests/btran_validation_test.cpp)
target_link_libraries(indigenous_phase4_btran_validation_test PRIVATE indigenous_phase4_basis)

add_executable(indigenous_phase4_basis_factorization_validation_test
  tests/basis_factorization_validation_test.cpp
)
target_link_libraries(indigenous_phase4_basis_factorization_validation_test PRIVATE indigenous_phase4_basis)

enable_testing()
add_test(NAME indigenous_phase4_basis_solver COMMAND indigenous_phase4_basis_test)
add_test(NAME indigenous_phase4_ftran_validation COMMAND indigenous_phase4_ftran_validation_test)
add_test(NAME indigenous_phase4_btran_validation COMMAND indigenous_phase4_btran_validation_test)
add_test(NAME indigenous_phase4_basis_factorization_validation
         COMMAND indigenous_phase4_basis_factorization_validation_test)

include(CheckLanguage)
check_language(CUDA)

if(CMAKE_CUDA_COMPILER)
  enable_language(CUDA)
  message(STATUS "Phase 4.6 backend: CUDA toolchain detected")
  find_package(CUDAToolkit REQUIRED)

  add_library(indigenous_phase4_cuda_basis src/gpu/cuda_basis_workspace.cu)
  target_include_directories(indigenous_phase4_cuda_basis
    PUBLIC include ../indigenous_solver_phase1/include
  )
  target_link_libraries(indigenous_phase4_cuda_basis
    PUBLIC CUDA::cudart CUDA::cusparse
  )
  set_target_properties(indigenous_phase4_cuda_basis PROPERTIES
    CUDA_STANDARD 17
    CUDA_STANDARD_REQUIRED ON
  )

  add_executable(indigenous_phase4_cuda_basis_representation_test
    tests/cuda_basis_representation_test.cu
  )
  target_link_libraries(indigenous_phase4_cuda_basis_representation_test
    PRIVATE indigenous_phase4_cuda_basis
  )
  add_test(NAME indigenous_phase4_cuda_basis_representation
           COMMAND indigenous_phase4_cuda_basis_representation_test)

  add_executable(indigenous_phase4_cuda_ftran_validation_test
    tests/cuda_ftran_validation_test.cu
  )
  target_link_libraries(indigenous_phase4_cuda_ftran_validation_test
    PRIVATE indigenous_phase4_cuda_basis indigenous_phase4_basis
  )
  add_test(NAME indigenous_phase4_cuda_ftran_validation
           COMMAND indigenous_phase4_cuda_ftran_validation_test)

  add_executable(indigenous_phase4_cuda_btran_validation_test
    tests/cuda_btran_validation_test.cu
  )
  target_link_libraries(indigenous_phase4_cuda_btran_validation_test
    PRIVATE indigenous_phase4_cuda_basis indigenous_phase4_basis
  )
  add_test(NAME indigenous_phase4_cuda_btran_validation
           COMMAND indigenous_phase4_cuda_btran_validation_test)
else()
  message(STATUS "Phase 4.7 backend: CUDA toolchain unavailable; device representation target skipped")
endif()

