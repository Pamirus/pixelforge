// The filters split across 1 to 6 threads, on a 3840 x 2160 image (24.9 MB).
// The Mac used here has six cores: two fast ones and four slower, power-saving
// ones, so the speed-up does not grow in equal steps.
//
//     build/benchmarks/pixelforge_benchmarks --benchmark_filter='parallel_'

#include <benchmark/benchmark.h>

#include "pixelforge/filters.hpp"
#include "pixelforge/image.hpp"
#include "pixelforge/parallel.hpp"

#include <cstdint>

namespace {

using pixelforge::Image;

constexpr int width = 3840;
constexpr int height = 2160;

// The pixels' values do not change the work, so a black image will do. The
// argument is the number of threads.
void parallel_brightness(benchmark::State& state)
{
    Image image(width, height);
    const auto threads = static_cast<unsigned>(state.range(0));
    for (auto _ : state) {
        pixelforge::brightness_parallel(image, 20, threads);
        benchmark::ClobberMemory();
    }
    state.SetBytesProcessed(state.iterations() * static_cast<std::int64_t>(image.size()));
}

void parallel_grayscale(benchmark::State& state)
{
    Image image(width, height);
    const auto threads = static_cast<unsigned>(state.range(0));
    for (auto _ : state) {
        pixelforge::grayscale_parallel(image, threads);
        benchmark::ClobberMemory();
    }
    state.SetBytesProcessed(state.iterations() * static_cast<std::int64_t>(image.size()));
}

void parallel_blur_radius2(benchmark::State& state)
{
    Image image(width, height);
    const auto threads = static_cast<unsigned>(state.range(0));
    for (auto _ : state) {
        pixelforge::box_blur_parallel(image, 2, threads);
        benchmark::ClobberMemory();
    }
    state.SetBytesProcessed(state.iterations() * static_cast<std::int64_t>(image.size()));
}

} // namespace

// UseRealTime: the time that matters is the wall clock, not the CPU time that
// adds up over all threads.
BENCHMARK(parallel_brightness)->DenseRange(1, 6)->UseRealTime()->Unit(benchmark::kMillisecond);
BENCHMARK(parallel_grayscale)->DenseRange(1, 6)->UseRealTime()->Unit(benchmark::kMillisecond);
BENCHMARK(parallel_blur_radius2)->DenseRange(1, 6)->UseRealTime()->Unit(benchmark::kMillisecond);
