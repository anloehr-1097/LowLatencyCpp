cmake_minimum_required(VERSION 3.20)

project(LowLatencyCppMastery LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Emit compile_commands.json for tooling (clangd, IDE, etc.).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

# --- bench library -----------------------------------------------------
# Header-only (all templates / always_inline): an INTERFACE target carries the
# include path and usage requirements without producing an archive.
add_library(il_benchmark INTERFACE)
target_include_directories(il_benchmark INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/bench)

# --- bench executable -----------------------------------------------------
add_executable(bench bench/main.cpp)
target_link_libraries(bench PRIVATE il_benchmark)

# --- core library ------------------------------------------------------
add_library(core
  src/core/Particle.cpp
  src/core/SPSCQueue.cpp
  src/core/Arena.cpp
)
target_include_directories(core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/core/include)
target_link_libraries(core PUBLIC il_benchmark)

# --- core executable ------------------------------------------------------
add_executable(core_run src/core/main.cpp)
target_link_libraries(core_run PRIVATE core)

# --- tests ----------------------------------------------------------------
option(LLCM_BUILD_TESTS "Build unit tests" ON)
if(LLCM_BUILD_TESTS)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  enable_testing()

  foreach(test_name particle_tests spsc_queue_tests arena_tests)
    add_executable(${test_name} src/core/tests/${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE core GTest::gtest_main)
    add_test(NAME ${test_name} COMMAND ${test_name})
  endforeach()
endif()
