# Benchmarks of the C++ version. The C baseline has its own in baseline_c/.
add_executable(pixelforge_bench bench.cpp)
target_link_libraries(pixelforge_bench PRIVATE pixelforge_core pixelforge_warnings)
