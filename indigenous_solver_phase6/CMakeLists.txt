cmake_minimum_required(VERSION 3.20)
project(IndigenousSolverPhase6Api VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(../indigenous_solver_phase5 phase5_build)

add_executable(indigenous_phase6_api_server
  src/solver_api_server.cpp
)
target_include_directories(indigenous_phase6_api_server PRIVATE
  ../indigenous_solver_phase5/include
  ../indigenous_solver_phase1/include
  ../indigenous_solver_phase2/include
  ../indigenous_solver_phase3/include
  ../indigenous_solver_phase4/include
)
target_link_libraries(indigenous_phase6_api_server PRIVATE indigenous_phase5_pipeline ws2_32)
if(MSVC)
  target_compile_definitions(indigenous_phase6_api_server PRIVATE _CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
