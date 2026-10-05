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

# Stricter warnings applied per-target to first-party code only, so
# third-party code (GTest sources, etc.) is never poisoned by -Werror.
function(llcm_strict_warnings target)
  target_compile_options(${target} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wshadow
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Wcast-align
    -Woverloaded-virtual
    -Wnull-dereference
    -Werror
  )
endfunction()

# --- sanitizers -----------------------------------------------------------
# ASan + UBSan for Debug builds (the typical dev config). On by default;
# disable with -DLLCM_ENABLE_SANITIZERS=OFF. LSan is unsupported on macOS
# and TSan is mutually exclusive with ASan, hence address+undefined only.
option(LLCM_ENABLE_SANITIZERS "Enable ASan/UBSan in Debug builds" ON)

function(llcm_enable_sanitizers target)
  if(NOT LLCM_ENABLE_SANITIZERS)
    return()
  endif()
  if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    return()
  endif()
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    return()
  endif()
  target_compile_options(${target} PRIVATE
    -fsanitize=address,undefined
    -fno-omit-frame-pointer
  )
  target_link_options(${target} PRIVATE
    -fsanitize=address,undefined
  )
endfunction()

# --- native architecture flags ------------------------------------------
# Compile first-party code for the build host's ISA (AVX2/AVX-512 on Intel,
# NEON on Apple Silicon / ARM Linux). Prefers -mcpu=native on ARM, where
# GCC/Clang favor it; -march=native elsewhere. Unsupported flags are probed
# and skipped, so untested compilers (MSVC, etc.) fall back to the baseline.
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-mcpu=native" LLCM_HAS_MCPU_NATIVE)
check_cxx_compiler_flag("-march=native" LLCM_HAS_MARCH_NATIVE)

function(llcm_native_arch target)
  if(LLCM_HAS_MCPU_NATIVE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    target_compile_options(${target} PRIVATE -mcpu=native)
  elseif(LLCM_HAS_MARCH_NATIVE)
    target_compile_options(${target} PRIVATE -march=native)
  endif()
endfunction()

# --- bench library -----------------------------------------------------
# Header-only (all templates / always_inline): an INTERFACE target carries the
# include path and usage requirements without producing an archive.
add_library(il_benchmark INTERFACE)
target_include_directories(il_benchmark INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/bench)

# --- bench executable -----------------------------------------------------
add_executable(bench bench/main.cpp)
target_link_libraries(bench PRIVATE il_benchmark)
llcm_strict_warnings(bench)
llcm_enable_sanitizers(bench)
llcm_native_arch(bench)

# --- core library ------------------------------------------------------
add_library(core
  src/core/Particle.cpp
  src/core/SPSCQueue.cpp
  src/core/Arena.cpp
  src/core/linalg.cpp
)
target_include_directories(core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/core/include)
target_link_libraries(core PUBLIC il_benchmark)
llcm_strict_warnings(core)
llcm_enable_sanitizers(core)
llcm_native_arch(core)

# --- core executable ------------------------------------------------------
add_executable(core_run src/core/main.cpp)
target_link_libraries(core_run PRIVATE core)
llcm_strict_warnings(core_run)
llcm_enable_sanitizers(core_run)
llcm_native_arch(core_run)

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

  foreach(test_name particle_tests spsc_queue_tests arena_tests pool_tests 
          arena_allocator_test fixed_point_tests linalg_tests )
    add_executable(${test_name} src/core/tests/${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE core GTest::gtest_main)
    llcm_strict_warnings(${test_name})
    llcm_enable_sanitizers(${test_name})
    llcm_native_arch(${test_name})
    add_test(NAME ${test_name} COMMAND ${test_name})
  endforeach()
endif()
