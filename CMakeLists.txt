cmake_minimum_required(VERSION 3.25)

project(pixelforge
  VERSION 0.0.0
  DESCRIPTION "Photo filters in C and C++20, rewritten stage by stage and benchmarked"
  HOMEPAGE_URL "https://github.com/Pamirus/pixelforge"
  LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# The project is about measuring speed, so a build without a type is a Release build.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(PIXELFORGE_BUILD_TESTS "Build the unit tests (downloads GoogleTest)" ON)

# Warning flags for the project's own targets. Third-party code does not use them.
add_library(pixelforge_warnings INTERFACE)
if(MSVC)
  target_compile_options(pixelforge_warnings INTERFACE /W4 /permissive-)
else()
  target_compile_options(pixelforge_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow)
endif()

add_subdirectory(third_party/stb)

if(PIXELFORGE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
