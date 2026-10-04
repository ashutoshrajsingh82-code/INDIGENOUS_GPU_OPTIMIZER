cmake_minimum_required(VERSION 3.20)
project(IndigenousSolverPhase3 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CheckLanguage)
check_language(CUDA)

if(CMAKE_CUDA_COMPILER)
  enable_language(CUDA)
  add_library(indigenous_gpu_backend
    src/gpu/vector_ops.cu
  )
  target_include_directories(indigenous_gpu_backend PUBLIC include)
  set_target_properties(indigenous_gpu_backend PROPERTIES
    CUDA_STANDARD 17
    CUDA_STANDARD_REQUIRED ON
    CUDA_EXTENSIONS OFF
  )

  add_executable(indigenous_gpu_smoke_test tests/gpu_smoke_test.cu)
  target_link_libraries(indigenous_gpu_smoke_test PRIVATE indigenous_gpu_backend)
  set_target_properties(indigenous_gpu_smoke_test PROPERTIES
    CUDA_STANDARD 17
    CUDA_STANDARD_REQUIRED ON
    CUDA_EXTENSIONS OFF
  )

  enable_testing()
  add_test(NAME indigenous_gpu_smoke_test COMMAND indigenous_gpu_smoke_test)
  message(STATUS "Phase 3 CUDA backend: ENABLED")
else()
  message(STATUS "Phase 3 CUDA backend: DISABLED (CUDA compiler not found)")
endif()
