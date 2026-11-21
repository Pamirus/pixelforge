# Benchmarks of the C++ version. The C baseline has its own in baseline_c/.
add_executable(pixelforge_bench bench.cpp)
target_link_libraries(pixelforge_bench PRIVATE pixelforge_core pixelforge_warnings)

# Microbenchmarks with Google Benchmark, downloaded at configure time.
if(PIXELFORGE_BUILD_BENCHMARKS)
  include(FetchContent)
  FetchContent_Declare(googlebenchmark
    URL https://github.com/google/benchmark/archive/refs/tags/v1.10.0.tar.gz
    URL_HASH SHA256=acc5c7c0fbed855c4aaa2d7c3c36fb82b0c15c28f4e151e3f679c2fc0aa404af
    SYSTEM)
  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_INSTALL_TOOLS OFF CACHE BOOL "" FORCE)
  # A warning from a newer compiler in the library must not break our build.
  set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googlebenchmark)

  add_executable(pixelforge_benchmarks copy_move.cpp views.cpp pixel_types.cpp
    compile_time.cpp dispatch.cpp layout.cpp)
  target_link_libraries(pixelforge_benchmarks PRIVATE
    pixelforge_core benchmark::benchmark_main pixelforge_warnings)
endif()
