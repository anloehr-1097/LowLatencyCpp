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

# --- core executable ------------------------------------------------------
add_executable(core src/core/main.cpp src/core/Particle.cpp)
target_include_directories(core PRIVATE src/core)
target_link_libraries(core PRIVATE il_benchmark)


